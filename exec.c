#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i2x.h"

#define DEBUG(args...)	printf(args)
#define DEBUG(args...)	{}

/* https://elixir.bootlin.com/linux/latest/source/drivers/i2c/i2c-dev.c#L235 */
static int dummy_ioctl(int _fd, unsigned long _req,
		struct i2c_rdwr_ioctl_data *msgset)
{
	static size_t ri = 0;
	static char random_data[] = {
		0x08, 0x07, 0x3d, 0xb7, 0x72, 0xf7, 0x11, 0x3c,
		0x1b, 0x46, 0xcc, 0xcd, 0x8c, 0xbe, 0xae, 0x0c,
		0x91, 0x7b, 0x9d, 0x6f, 0x74, 0xcf, 0x58, 0x40,
		0x55, 0xfb, 0x0f, 0x04, 0x17, 0x7c, 0xd1, 0xac
	};

	for (size_t i = 0; i < msgset->nmsgs; ++i) {
		DEBUG("  ioctl(%s) [%2d] :",
		      msgset->msgs[i].flags & I2C_M_RD ? "R" : "W",
		      msgset->msgs[i].len);
		for (size_t j = 0; j < msgset->msgs[i].len; ++j) {
			if (msgset->msgs[i].flags & I2C_M_RD)
				msgset->msgs[i].buf[j] = random_data[(j + ri) % 32];
			else
				random_data[ri] ^= msgset->msgs[i].buf[j];

			ri = (ri + 1) % 32;
			DEBUG(" %02hhx", msgset->msgs[i].buf[j]);
		}
		DEBUG("\n");
	}

	return 0;
}

static void print_bytes(uint8_t *buf, size_t len, char *fmt,
                        int space, int newline)
{
	for (size_t i = 0; i < len; ++i) {
                printf(fmt, buf[i]);
                if (space && i < len - 1)
                        putchar(' ');
        }
        if (newline)
                putchar('\n');
}

static void print_filler(size_t len, char *fill)
{
        while (len--)
                printf("%s%s", fill, len ? " " : "");
}

static void print_prefix(struct i2x_prog *p,
                         struct i2x_segment *segment, size_t i, uint16_t addr)
{
        if (p->verbose) {
                printf("%02"PRIx8":", addr & 0xff);
                printf(segment->msgflags[i] & I2X_MSG_RD ? "R " : "W ");
        } else if (i > 0 && segment->msgflags[i - 1] & I2X_MSG_REG) {
                print_bytes(segment->msgset.msgs[i - 1].buf,
                            segment->msgset.msgs[i - 1].len, "%"PRIx8, 1, 0);
                printf(": ");
        }
}

static void print_segment(struct i2x_prog *p, struct i2x_segment *segment,
                          uint16_t addr)
{
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;
                // uint32_t flags = segment->msgflags[i];
		if (!(segment->msgflags[i] & I2X_MSG_RD) && !p->verbose)
			continue;
                if (p->verbose && !i)
                        printf("S  ");

                switch(p->output_format) {
                case HEX:
                        print_prefix(p, segment, i, addr);
                        if (p->dry_run && segment->msgflags[i] & I2X_MSG_RD)
                                print_filler(msg->len, "..");
                        else
                                print_bytes(msg->buf, msg->len, "%02"PRIx8,
                                            1, !p->verbose);
                        break;
                case DEC:
                        print_prefix(p, segment, i, addr);
                        if (p->dry_run && segment->msgflags[i] & I2X_MSG_RD)
                                print_filler(msg->len, "...");
                        else
                                print_bytes(msg->buf, msg->len, "%3"PRIu8,
                                            1, !p->verbose);
                        break;
                /* no verbosity */
                case PLAIN:
                        print_bytes(msg->buf, msg->len, "%02"PRIx8, 0, 0);
                        break;
                case RAW:
                        write(STDOUT_FILENO, msg->buf, msg->len);
                        break;
                }

                if (p->verbose)
                        printf(i < segment->msgset.nmsgs - 1
                                ? "\nSr " : " P\n\n");
	}
}

static void exec_segment(struct i2x_prog *p, struct i2x_segment *segment,
                         uint16_t addr, uint8_t *reg, size_t reg_width)
{
        /* set address, fill in registers, zero read buffers */
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;

		msg->addr = addr;
		if (segment->msgflags[i] & I2X_MSG_RD)
			explicit_bzero(msg->buf, msg->len);
		if (segment->msgflags[i] & I2X_MSG_REG)
			memcpy(msg->buf, reg, reg_width);
	}

        if (!p->dry_run) {
                if (dummy_ioctl(0, I2C_RDWR, &segment->msgset))
                        errx(1, "oh no! ioctl failed!");
        }

        print_segment(p, segment, addr);
	if (segment->delay)
		usleep(1000 * segment->delay);
}

static void exec_segment_list(struct i2x_prog *p, struct i2x_list *segment_list,
                              uint8_t *reg, size_t reg_width)
{
	i2x_list_foreach (i2x_segment, segment, segment_list)
		exec_segment(p, segment, 0, reg, reg_width);
}

static void exec_cmd(struct i2x_prog *p, struct i2x_cmd *cmd)
{
	struct i2x_list *reg_spec = cmd->reg_spec;
	struct i2x_list *segment_list = cmd->segment_list;

	if (!reg_spec) {
		exec_segment_list(p, segment_list, NULL, 0);
		return;
	}

	assert(reg_spec->len);
	size_t width = i2x_list_get(i2x_regrange, reg_spec, 0)->width;

	uint8_t *reg = malloc(width);
	assert(reg);
	i2x_list_foreach (i2x_regrange, range, reg_spec) {
		memcpy(reg, range->start, width);
		do {
			exec_segment_list(p, segment_list, reg, width);
			incn(reg, width, range->big_endian);
		} while(memcmp(reg, range->stop, width));
	}
	free(reg);
}

void i2x_exec_prog(struct i2x_prog *prog)
{
	i2x_list_foreach (i2x_cmd, cmd, prog->cmd_list)
		exec_cmd(prog, cmd);
}

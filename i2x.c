#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "i2x.h"

/* add 1 with carry and wrap around (unsigned) */
void incn(uint8_t *n, int width, int big_endian)
{
	for (size_t b = 0; b < width; ++b)
		if (++n[big_endian ? width-1 - b : b]) break;
}

/**
 * i2x_cmd_segment() - Translate message list into segments for the kernel.
 * @msg_list: list of i2x_msgs to be segmented
 * @reg_spec: register spec of the messages in the list
 *
 * 
 */
struct i2x_list *i2x_cmd_segment(struct i2x_list *msg_list,
				 struct i2x_list *reg_spec)
{
	struct i2x_list *segment_list = i2x_list_make();

	size_t reg_width = 0;
	if (reg_spec) {
		reg_width = i2x_list_get(i2x_regrange, reg_spec, 0)->width;
		assert(reg_width);
	}

	size_t nrd = 0, nseg = 0, npause = 0;
	i2x_list_foreach (i2x_msg, msg, msg_list) {
		if (msg->flags & F_MSG_RD) {
			++nrd;
			/*
			 * If a register address write is prepended, then
			 * add a stop to the previous message, unless it is
			 * explicitly marked as a repeated start.
			 */
			if (reg_spec && msg_prev &&
					!(msg_prev->flags & F_MSG_SR)) {
				msg_prev->flags |= F_MSG_STOP;
				++nseg;
			}
		}
		if (msg->flags & F_MSG_STOP)
			++nseg;
		if (msg->flags & F_MSG_PAUSE)
			++nseg, ++npause;
	}

	/* an additional Wr for each Rd if it is prefixed by register */
	size_t n = msg_list->len + nrd - npause;
	struct i2c_msg *kmsgs = calloc(n, sizeof(struct i2c_msg));
	uint16_t *msgflags = calloc(n, sizeof(uint16_t));
	assert(kmsgs && msgflags);

	size_t next_segment = 0, k = 0;
	i2x_list_foreach (i2x_msg, msg, msg_list) {
		if (msg->flags & F_MSG_PAUSE) {
			i2x_list_get(i2x_segment, segment_list,
			             segment_list->len - 1)->delay = msg->len;
			continue;
		}

		/* insert write for register pointer if read (2 msg total) */
		if (reg_spec && msg->flags & F_MSG_RD) {
			kmsgs[k].addr = 0;
			kmsgs[k].len = reg_width;
			kmsgs[k].buf = malloc(reg_width);
			assert(kmsgs[k].buf);
			msgflags[k] = F_MSG_REG;
			++k;
		}

		kmsgs[k].addr = 0;
		kmsgs[k].flags = msg->flags & F_MSG_RD ? I2C_M_RD : 0;
		kmsgs[k].len = msg->len;
		kmsgs[k].buf = msg->buf;
		msgflags[k] = msg->flags;

		/* prepend to msg buffer if write (1 msg total) */
		if (reg_spec && !(msg->flags & F_MSG_RD)) {
			uint8_t *buf = malloc(reg_width + msg->len);
			assert(buf);

			memcpy(buf + reg_width, msg->buf, msg->len);
			kmsgs[k].len += reg_width;
			kmsgs[k].buf = buf;
			free(msg->buf);
			msgflags[k] |= F_MSG_REG;
		}

		if (msg->flags & F_MSG_STOP) {
			struct i2x_segment *segment =
				i2x_segment_make(kmsgs + next_segment,
						 msgflags + next_segment,
						 k + 1 - next_segment);
			i2x_list_extend(segment_list, segment);
			next_segment = k + 1;
		}
		++k;
		// fill in spots for reg
		// msg metadata (delays, read format, ...) how???
	}

	return segment_list;
}

struct i2x_cmd *i2x_cmd_make(struct i2x_list *msg_list,
			     struct i2x_list *reg_spec)
{
	assert(msg_list);
	struct i2x_cmd *cmd = malloc(sizeof(struct i2x_cmd));
	assert(cmd);

	/* last message in msg_list necessarily has a stop */
	i2x_list_get(i2x_msg, msg_list, msg_list->len - 1)->flags |= F_MSG_STOP;

	cmd->segment_list = i2x_cmd_segment(msg_list, reg_spec);
	cmd->reg_spec = reg_spec;

	return cmd;
}

struct i2x_segment *i2x_segment_make(struct i2c_msg *msgs, uint16_t *msgflags,
				     int nmsgs)
{
	struct i2x_segment *segment = malloc(sizeof(struct i2x_segment));
	assert(segment);

	segment->msgset.msgs = msgs;
	segment->msgset.nmsgs = nmsgs;
	segment->msgflags = msgflags;

	return segment;
}

struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len, uint16_t flags)
{
	assert(flags & F_MSG_PAUSE || len);
	struct i2x_msg *msg = malloc(sizeof(struct i2x_msg));
	assert(msg);

	msg->buf = buf;
	msg->len = len;
	msg->flags = flags;

	if (flags & F_MSG_RD) {
		/* allocate buffer now for reads */
		msg->buf = malloc(len);
		assert(msg->buf);
	}
	return msg;
}

/* consume two literals and produce a register range */
struct i2x_regrange *i2x_regrange_make(struct i2x_literal *start,
				       struct i2x_literal *stop)
{
	assert(start && stop);
	assert(start->len == stop->len);
	struct i2x_regrange *regrange = malloc(sizeof(struct i2x_regrange));
	assert(regrange);

	regrange->start = start->buf;
	regrange->stop = stop->buf;
	regrange->width = start->len;
	/* default BE for hex, LE for dec: determined by first addr in regrange */
	regrange->big_endian = start->type & 1;

	if (regrange->start == regrange->stop) {
		regrange->stop = malloc(regrange->width);
		assert(regrange->stop);
		memcpy(regrange->stop, regrange->start, regrange->width);
	}
	/* to make looping easier */
	incn(regrange->stop, regrange->width, regrange->big_endian);
	return regrange;
}

struct i2x_literal *i2x_literal_extend(struct i2x_literal *dst,
				       struct i2x_literal *src)
{
	assert(dst && src);
	size_t new_len = dst->len + src->len;
	uint8_t *buf = realloc(dst->buf, new_len);
	assert(buf);
	memcpy(dst->buf + dst->len, src->buf, src->len);
	dst->len += src->len;
	dst->val = 0;
	return dst;
}

struct i2x_list *i2x_list_make()
{
	struct i2x_list *list = malloc(sizeof(struct i2x_list));
	assert(list);

	list->array = malloc(LIST_INIT_CAP * sizeof(void *));
	assert(list->array);

	list->len = 0;
	list->cap = LIST_INIT_CAP;

	return list;
}

struct i2x_list *i2x_list_extend(struct i2x_list *list, void *element)
{
	assert(list && list->len <= list->cap);
	if (list->len == list->cap) {
		void **a = realloc(list->array, 2 * list->cap * sizeof(void *));
		assert(a);
		list->array = a;
		list->cap <<= 1;
	}

	list->array[list->len] = element;
	assert(list->len != -1);
	++list->len;
	return list;
}

void i2x_list_free(struct i2x_list *list)
{
	assert(list);
	for (size_t i = 0; i < list->len; ++i)
		free(list->array[i]);
	free(list);
}

/******************************************************************************/

#define DEBUG(args...)	printf(args)

/* https://elixir.bootlin.com/linux/latest/source/drivers/i2c/i2c-dev.c#L235 */
int dummy_ioctl(int _fd, unsigned long _req,
		struct i2c_rdwr_ioctl_data *msgset)
{
	static const char random_data[] = {
		0x08, 0x07, 0x3d, 0xb7, 0x72, 0xf7, 0x11, 0x3c,
		0x1b, 0x46, 0xcc, 0xcd, 0x8c, 0xbe, 0xae, 0x0c,
		0x91, 0x7b, 0x9d, 0x6f, 0x74, 0xcf, 0x58, 0x40,
		0x55, 0xfb, 0x0f, 0x04, 0x17, 0x7c, 0xd1, 0xac
	};

	for (size_t i = 0; i < msgset->nmsgs; ++i) {
		if (msgset->msgs[i].flags & I2C_M_RD) {
			for (size_t j = 0; j < msgset->msgs[i].len; ++j)
				msgset->msgs[i].buf[j] = random_data[j % 32];
		}

		DEBUG("  ioctl(%s) [%2d] :",
		      msgset->msgs[i].flags & I2C_M_RD ? "R" : "W",
		      msgset->msgs[i].len);
		for (size_t j = 0; j < msgset->msgs[i].len; ++j) {
			DEBUG(" %02hhx", msgset->msgs[i].buf[j]);
		}
		DEBUG("\n");
	}

	return 0;
}

void print_shexp(uint8_t *buf, size_t len) {
	for (size_t i = 0; i < len; ++i)
		printf("%02x%s", buf[i], i < len - 1 ? " " : "");
}

void i2x_exec_segment(struct i2x_segment *segment, uint16_t addr,
		      uint8_t *reg, size_t reg_width)
{
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;

		msg->addr = addr;
		if (segment->msgflags[i] & F_MSG_RD)
			explicit_bzero(msg->buf, msg->len);
		if (segment->msgflags[i] & F_MSG_REG)
			memcpy(msg->buf, reg, reg_width);
	}

	if (dummy_ioctl(0, I2C_RDWR, &segment->msgset))
		errx(1, "oh no! ioctl failed!");

	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;
		if (!(segment->msgflags[i] & F_MSG_RD))
			continue;

		if (i > 0 && segment->msgflags[i - 1] & F_MSG_REG) {
			print_shexp(segment->msgset.msgs[i - 1].buf,
				    segment->msgset.msgs[i - 1].len);
			printf(": ");
		}

		print_shexp(msg->buf, msg->len);
		putchar('\n');
	}

	if (segment->delay)
		usleep(1000 * segment->delay);
}

void i2x_exec_segment_list(struct i2x_list *segment_list, uint8_t *reg,
				size_t reg_width)
{
	i2x_list_foreach (i2x_segment, segment, segment_list)
		i2x_exec_segment(segment, 0, reg, reg_width);
}

void i2x_exec_cmd(struct i2x_cmd *cmd)
{
	struct i2x_list *reg_spec = cmd->reg_spec;
	struct i2x_list *segment_list = cmd->segment_list;

	if (!reg_spec) {
		i2x_exec_segment_list(segment_list, NULL, 0);
		return;
	}

	assert(reg_spec->len);
	size_t width = i2x_list_get(i2x_regrange, reg_spec, 0)->width;

	uint8_t *reg = malloc(width);
	assert(reg);
	i2x_list_foreach (i2x_regrange, range, reg_spec) {
		memcpy(reg, range->start, width);
		do {
			i2x_exec_segment_list(segment_list, reg, width);
			incn(reg, width, range->big_endian);
		} while(memcmp(reg, range->stop, width));
	}
	free(reg);
}

void i2x_exec_prog(struct i2x_prog *prog)
{
	i2x_list_foreach (i2x_cmd, cmd, prog->cmd_list)
		i2x_exec_cmd(cmd);
}

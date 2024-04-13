#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2x.h"

static void iprintf(int indent, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("%*s", indent, "");
	vprintf(fmt, ap);
	va_end(ap);
}

static void dump_segment(struct i2x_segment *segment, int reg_width)
{
	iprintf(8, "SEGMENT [%d]\n", segment->msgset.nmsgs);
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;
		uint32_t flags = segment->msgflags[i];
		iprintf(10, "MSG [%s%3d]",
			flags & I2X_MSG_RD ? "R" : "W", msg->len);

		if (!(flags & I2X_MSG_RD)) {
			for (size_t j = 0; j < msg->len; ++j) {
				if (j < reg_width && flags & I2X_MSG_REG)
					printf(" rr");
				else
					printf(" %02hhx", msg->buf[j]);
			}
		}
		putchar('\n');
	}
	if (segment->delay)
		iprintf(10, "(%4dms delay)\n", segment->delay);
}

static void dump_segment_list(struct i2x_list *segment_list, int reg_width)
{
	iprintf(6, "SEGMENTS\n");
	i2x_list_foreach (i2x_segment, segment, segment_list)
		dump_segment(segment, reg_width);
}

static void dump_reg_spec(struct i2x_list *reg_spec)
{
	iprintf(6, "REGSPEC\n");
	i2x_list_foreach (i2x_regrange, regrange, reg_spec) {
		iprintf(6, "- REGRANGE [");
		for (size_t j = 0; j < regrange->width; ++j)
			printf("%02hhx", regrange->start[j]);
		printf("-");
		for (size_t j = 0; j < regrange->width; ++j)
			printf("%02hhx", regrange->stop[j]);
		puts(")");
	}
}

static void dump_cmd_list(struct i2x_list *cmd_list)
{
	iprintf(2, "CMD_LIST\n");
	i2x_list_foreach (i2x_cmd, cmd, cmd_list) {
		iprintf(2, "- CMD\n");
		if (cmd->reg_spec)
			dump_reg_spec(cmd->reg_spec);
		size_t width = cmd->reg_spec ?
			i2x_list_get(i2x_regrange, cmd->reg_spec, 0)->width : 0;
		dump_segment_list(cmd->segment_list, width);
	}
}

void i2x_dump_tree(struct i2x_prog *prog)
{
	iprintf(0, "PROG\n");
	iprintf(2, "TARGET [bus=%"PRIu32" addr=%"PRIx8"]\n",
		prog->target->bus, prog->target->address);
	dump_cmd_list(prog->cmd_list);
}

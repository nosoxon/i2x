#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2x.h"

void iprintf(int indent, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	printf("%*s", indent, "");
	vprintf(fmt, ap);
	va_end(ap);
}

void dump_segment(struct i2x_segment *segment, int reg_width)
{
	iprintf(6, "i2x_segment [%d]\n", segment->msgset.nmsgs);
	for (size_t i = 0; i < segment->msgset.nmsgs; ++i) {
		struct i2c_msg *msg = segment->msgset.msgs + i;
		uint32_t flags = segment->msgflags[i];
		iprintf(10, "i2c_msg [%s%3d]",
			flags & F_MSG_RD ? "R" : "W", msg->len);

		if (!(flags & F_MSG_RD)) {
			for (size_t j = 0; j < msg->len; ++j) {
				if (j < reg_width && flags & F_MSG_REG)
					printf(" rr");
				else
					printf(" %02hhx", msg->buf[j]);
			}
		}
		puts("");
	}
}

void dump_segment_list(struct i2x_list *segment_list, int reg_width)
{
	iprintf(6, "i2x_list(i2x_segment)\n");
	i2x_list_foreach (i2x_segment, segment, segment_list)
		dump_segment(segment, reg_width);
}

void dump_reg_spec(struct i2x_list *reg_spec)
{
	iprintf(6, "i2x_list(i2x_regrange)\n");
	i2x_list_foreach (i2x_regrange, regrange, reg_spec) {
		iprintf(8, "i2x_regrange [");
		for (size_t j = 0; j < regrange->width; ++j)
			printf("%02hhx", regrange->start[j]);
		printf("-");
		for (size_t j = 0; j < regrange->width; ++j)
			printf("%02hhx", regrange->stop[j]);
		puts("]");
	}
}

void dump_cmd_list(struct i2x_list *cmd_list)
{
	iprintf(2, "i2x_list(i2x_cmd)\n");
	i2x_list_foreach (i2x_cmd, cmd, cmd_list) {
		iprintf(4, "i2x_cmd\n");
		if (cmd->reg_spec)
			dump_reg_spec(cmd->reg_spec);
		size_t width = cmd->reg_spec ?
			i2x_list_get(i2x_regrange, cmd->reg_spec, 0)->width : 0;
		dump_segment_list(cmd->segment_list, width);
	}
}

void dump_prog(struct i2x_prog *prog)
{
	iprintf(0, "i2x_prog\n");
	iprintf(2, "i2x_target(bus=%"PRIu32", addr=%"PRIx8"\n",
		prog->target->bus, prog->target->address);
	dump_cmd_list(prog->cmd_list);
}

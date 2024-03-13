%{
#include "i2x.h"
#include "i2x.yy.h"

void dump_cmd_list(struct i2x_list *cmd_list);
%}

%union {
	struct i2x_literal	*literal;
	struct i2x_regrange	*regrange;
	struct i2x_msg		*msg;
	struct i2x_cmd		*cmd;
	struct i2x_list		*list;
}

%token WRITE READ
%token <literal> PHEX DECIMAL

%type <literal> const data_expr
%type <regrange> regrange
%type <msg> msg
%type <cmd> cmd
%type <list> regrange_list reg_spec msg_list cmd_list

%%

prog: cmd_list	{
	printf("\nDEBUG DUMP\n");
	dump_cmd_list($1);

	printf("\nBEGIN EXECUTE\n");
	i2x_exec_cmd_list($1);
};

cmd_list
	: cmd				{
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| cmd_list '/' cmd		{ $$ = i2x_list_extend($1, $3); }
	;

cmd
	: msg_list			{
		$$ = i2x_cmd_make($1, NULL);
		i2x_list_free($1);
	}
	| reg_spec msg_list		{
		$$ = i2x_cmd_make($2, $1);
		i2x_list_free($2);
	};

msg_list
	: msg				{
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| msg_list msg			{
		struct i2x_msg *m = i2x_list_get($1, $1->len - 1);
		/* implicit: repeated start if RAW, else stop */
		if (m->flags & F_MSG_RD || !($2->flags & F_MSG_RD))
			m->flags |= F_MSG_STOP;
		$$ = i2x_list_extend($1, $2);
	}
	| msg_list ',' msg		{
		// ((struct msg *) i2x_list_get($1, $1->len - 1))->flags |= F_MSG_SR;
		$$ = i2x_list_extend($1, $3);
	}
	| msg_list '.' msg		{
		((struct i2x_msg *) i2x_list_get($1, $1->len - 1))->flags |= F_MSG_STOP;
		$$ = i2x_list_extend($1, $3);
	};

msg
	: WRITE data_expr		{ $$ = i2x_msg_make($2->buf, $2->len); }
	| READ DECIMAL			{ $$ = i2x_msg_make(NULL, $2->val); }
	;

data_expr
	: const				{ $$ = $1; }
	| data_expr const		{
		$$ = i2x_literal_extend($1, $2);
		/* free literal we just copied */
		free($2->buf);
		free($2);
	};

reg_spec
	: regrange_list			{ $$ = $1; }
	| regrange_list ':'		{ $$ = $1; }
	;

regrange_list
	: regrange			{
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| regrange_list ',' regrange	{ $$ = i2x_list_extend($1, $3); }
	;

regrange
	: const				{ $$ = i2x_regrange_make($1, $1); }
	| const '-' const		{ $$ = i2x_regrange_make($1, $3); };

const
	: PHEX				{ $$ = $1; }
	| DECIMAL			{ $$ = $1; }
	;

%%

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void iprintf(int indent, const char* fmt, ...) {
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
		iprintf(8, "i2c_msg [%s%3d]",
			msg->flags & I2C_M_RD ? "R" : "W", msg->len);

		if (!(msg->flags & I2C_M_RD)) {
			for (size_t j = 0; j < msg->len; ++j) {
				if (j < reg_width && msg->flags & F_MSG_REG)
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
	iprintf(4, "i2x_list(i2x_segment)\n");
	for (size_t i = 0; i < segment_list->len; ++i) {
		struct i2x_segment *seg = i2x_list_get(segment_list, i);
		dump_segment(seg, reg_width);
	}
}

void dump_reg_spec(struct i2x_list *reg_spec)
{
	iprintf(4, "i2x_list(i2x_regrange)\n");
	for (size_t i = 0; i < reg_spec->len; ++i) {
		struct i2x_regrange *regrange = i2x_list_get(reg_spec, i);
		iprintf(6, "i2x_regrange [");
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
	iprintf(0, "i2x_list(i2x_cmd)\n");
	for (size_t i = 0; i < cmd_list->len; ++i) {
		struct i2x_cmd *cmd = i2x_list_get(cmd_list, i);
		iprintf(2, "i2x_cmd\n");
		if (cmd->reg_spec)
			dump_reg_spec(cmd->reg_spec);
		size_t width = cmd->reg_spec ?
			((struct i2x_regrange *) i2x_list_get(cmd->reg_spec, 0))
				->width : 0;
		dump_segment_list(cmd->segment_list, width);
	}
}

%{
#include "i2x.h"

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

prog: cmd_list	{ dump_cmd_list($1); };

cmd_list
	: cmd				{ $$ = i2x_list_make($1); }
	| cmd_list '/' cmd		{ $$ = i2x_list_extend($1, $3); }
	;

cmd
	: msg_list			{ $$ = i2x_cmd_make($1, NULL); }
	| reg_spec msg_list		{ $$ = i2x_cmd_make($2, $1); }
	;

msg_list
	: msg				{ $$ = i2x_list_make($1); }
	| msg_list msg			{
		struct i2x_msg *m = i2x_list_get($1, $1->len - 1);
		/* implicit: repeated start if RAW, else stop */
		if (!m->buf || $2->buf)
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
	: regrange			{ $$ = i2x_list_make($1); }
	| regrange_list ',' regrange	{ $$ = i2x_list_extend($1, $3); }
	;

regrange
	: const				{
		$$ = i2x_regrange_make($1, $1);
		free($1);
	}
	| const '-' const		{
		$$ = i2x_regrange_make($1, $3);
		free($1);
		free($3);
	};

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

void dump_msg_list(struct i2x_list *msg_list)
{
	iprintf(4, "i2x_list(i2x_msg)\n");
	for (size_t i = 0; i < msg_list->len; ++i) {
		struct i2x_msg *msg = msg_list->array[i];
		iprintf(6, "i2x_msg [%s%d]",
			msg->flags & F_MSG_RD ? "R" : "W", msg->len);
		if (!(msg->flags & F_MSG_RD))
			for (size_t j = 0; j < msg->len; ++j)
				printf(" %02hhx", msg->buf[j]);
		printf(" [%s]\n", msg->flags & F_MSG_STOP ? "P" : "Sr");
	}
}

void dump_reg_spec(struct i2x_list *reg_spec)
{
	iprintf(4, "i2x_list(i2x_regrange)\n");
	for (size_t i = 0; i < reg_spec->len; ++i) {
		struct i2x_regrange *regrange = reg_spec->array[i];
		iprintf(6, "i2x_regrange [");
		for (size_t j = 0; j < regrange->len; ++j)
			printf("%02hhx", regrange->lower[j]);
		printf("-");
		for (size_t j = 0; j < regrange->len; ++j)
			printf("%02hhx", regrange->upper[j]);
		puts("]");
	}
}

void dump_cmd_list(struct i2x_list *cmd_list)
{
	iprintf(0, "i2x_list(i2x_cmd)\n");
	for (size_t i = 0; i < cmd_list->len; ++i) {
		struct i2x_cmd *cmd = cmd_list->array[i];
		iprintf(2, "i2x_cmd\n");
		if (cmd->reg_spec)
			dump_reg_spec(cmd->reg_spec);
		dump_msg_list(cmd->msg_list);
	}
}

%{
#include <err.h>

#include "dump.h"
#include "i2x.h"
#include "i2x.yy.h"
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
	| cmd_list '/' cmd		{
		$$ = i2x_list_extend($1, $3);
	};

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
		/* if there is a regrange then every message is stop unless explicit comma*/
		struct i2x_msg *m = i2x_list_get(i2x_msg, $1, $1->len - 1);
		/* implicit: repeated start if RAW, else stop */
		if (m->flags & F_MSG_RD || !($2->flags & F_MSG_RD))
			m->flags |= F_MSG_STOP;
		$$ = i2x_list_extend($1, $2);
	}
	| msg_list ',' msg		{
		i2x_list_get(i2x_msg, $1, $1->len - 1)->flags |= F_MSG_SR;
		$$ = i2x_list_extend($1, $3);
	}
	| msg_list '.' msg		{
		i2x_list_get(i2x_msg, $1, $1->len - 1)->flags |= F_MSG_STOP;
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
	| regrange_list ',' regrange	{
		if ($3->width != i2x_list_get(i2x_regrange, $1, 0)->width)
			errx(1, "register ranges must be of explicit equal width");
		$$ = i2x_list_extend($1, $3);
	};

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

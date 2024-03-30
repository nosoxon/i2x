%define api.pure full

%{
#include <assert.h>
#include <err.h>

#include "i2x.h"
#include "i2x.tab.h"
#include "i2x.yy.h"
%}

%lex-param {void *scanner}
%parse-param {void *scanner} {void **prog}

%union {
	struct i2x_target	*target;
	struct i2x_literal	*literal;
	struct i2x_regrange	*regrange;
	struct i2x_msg		*msg;
	struct i2x_cmd		*cmd;
	struct i2x_list		*list;
}

%token <target> TARGET
%token WRITE READ PAUSE
%token <literal> PHEX DECIMAL

%type <literal> const data_expr
%type <regrange> regrange
%type <msg> msg
%type <cmd> cmd
%type <list> regrange_list reg_spec msg_list cmd_list

%%

prog:	TARGET cmd_list			{
	struct i2x_prog *p = malloc(sizeof(struct i2x_prog));
	assert(p);
	p->target = $1;
	p->cmd_list = $2;
	*prog = p;
};

cmd_list: cmd				{
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| cmd_list '/' cmd		{
		$$ = i2x_list_extend($1, $3);
	};

cmd	: msg_list			{
		$$ = i2x_cmd_make($1, NULL);
		i2x_list_free($1);
	}
	| reg_spec msg_list		{
		$$ = i2x_cmd_make($2, $1);
		i2x_list_free($2);
	};

msg_list: msg				{
		if ($1->flags & F_MSG_PAUSE)
			errx(1, "first message in a command cannot be a pause");
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| msg_list msg			{
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

msg	: WRITE data_expr		{
		$$ = i2x_msg_make($2->buf, $2->len, 0);
	}
	| READ DECIMAL			{
		$$ = i2x_msg_make(NULL, $2->val, F_MSG_RD);
	}
	| PAUSE DECIMAL			{
		$$ = i2x_msg_make(NULL, $2->val, F_MSG_PAUSE);
	};

data_expr
	: const				{ $$ = $1; }
	| data_expr const		{
		$$ = i2x_literal_extend($1, $2);
		/* free literal we just copied */
		free($2->buf);
		free($2);
	};

reg_spec: regrange_list			{ $$ = $1; }
	| regrange_list ':'		{ $$ = $1; }
	;

regrange_list
	: regrange			{
		struct i2x_list *list = i2x_list_make();
		$$ = i2x_list_extend(list, $1);
	}
	| regrange_list ',' regrange	{
		/* ensure all register ranges within a regspec are of equal width */
		if ($3->width != i2x_list_get(i2x_regrange, $1, 0)->width)
			errx(1, "register ranges must be of explicit equal width");
		$$ = i2x_list_extend($1, $3);
	};

regrange: const				{
		$$ = i2x_regrange_make($1, $1);
		free($1);
	}
	| const '-' const		{
		$$ = i2x_regrange_make($1, $3);
		free($1);
		free($3);
	};

const	: PHEX				{ $$ = $1; }
	| DECIMAL			{ $$ = $1; }
	;

%{
#include <stdio.h>
#include <inttypes.h>
#include "i2x.h"
#include "ast.h"

struct ast_node* ast_node_make(int type, void* lval, void* rval, size_t len);
struct ast_node* ast_list_extend(struct ast_node* head, struct ast_node* val);

void dump_ast(int indent, struct ast_node* node, int first);
%}

%union {
	// struct const_dec*	dec;
	// struct const_hex*	hex;
	// struct ast_node*	node;
	struct node*	node;
}

%token WRITE READ
%token <node> PHEX DECIMAL

%type <node> reg_expr reg_expr_list reg_spec
%type <node> const data_expr msg msg_list cmd cmd_list

%%

prog: cmd_list	{
		dump_ast(0, $1, 1);
		struct prog *p = xlate_prog($1);
		exec_prog(p);
	};

cmd_list
	: cmd				{ $$ = inode_make(T_CMD_LIST, $1, NULL, 1); }
	| cmd_list '/' cmd		{ $$ = inode_extend($1, $3); }
	;

cmd
	: msg_list			{ $$ = inode_make(T_CMD, $1, NULL, 1); }
	| reg_spec msg_list		{ $$ = inode_make(T_CMD, $2, $1, 1); }
	;

msg_list
	: msg				{ $$ = inode_make(T_MSG, $1, NULL, 1); }
	| msg_list msg			{ $$ = inode_extend($1, $2); }
	| msg_list ',' msg		{ inode_extend($1,
						inode_make(T_REP_START, NULL, NULL, 1));
					  $$ = inode_extend($1, $3); }
	| msg_list '.' msg		{ inode_extend($1,
						inode_make(T_STOP, NULL, NULL, 1));
					  $$ = inode_extend($1, $3); }
	;

msg
	: WRITE data_expr		{ $$ = inode_make(T_WRITE_EXPR, $2, NULL, 1); }
	| READ DECIMAL			{ $$ = inode_make(T_READ_EXPR, $2, NULL, 1); }
	;

data_expr
	: const				{ $$ = inode_make(T_DATA_EXPR, $1, NULL, 1); }
	| data_expr const		{ $$ = inode_extend($1, $2); }
	;

reg_spec
	: reg_expr_list			{ $$ = $1; }
	| reg_expr_list ':'		{ $$ = $1; }
	;

reg_expr_list
	: reg_expr			{ $$ = inode_make(T_REG_SPEC, $1, NULL, 1); }
	| reg_expr_list ',' reg_expr	{ $$ = inode_extend($1, $3); }
	;

reg_expr
	: const				{ $$ = inode_make(T_REG, $1, $1, 1); }
	| const '-' const		{ $$ = inode_make(T_REG, $1, $3, 1); }
	;

const
	: PHEX				{ $$ = $1; }
	| DECIMAL			{ $$ = $1; }


%%

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

struct node* inode_make(uint16_t type, struct node *lval, struct node *rval, size_t len) {
	struct node *node = malloc(sizeof(struct node));
	/* TODO alloc check */
	node->type = type;
	node->lval.n = lval;
	node->rval.n = rval;
	node->len = len;
	return node;
}

struct node* inode_extend(struct node *head, struct node *val) {
	struct node *prev = head;
	for (; head->rval.n; prev = prev->rval.n);
	prev->rval = inode_make(head->type, val, NULL, 1);
}

/* extern char *yytext; */


void iprintf(int indent, const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	printf("%*s", indent, "");
	vprintf(fmt, ap);
	va_end(ap);
}

void dump_ast(int indent, struct ast_node* node, int first) {
	switch (node->type) {
	case T_HEX:
	case T_DEC:

		struct const_hex* hex = node->lval;
		iprintf(indent, "HEX [%d] ", node->len);
		for (int i = 0; i < hex->len; ++i)
			printf("%02hhx ", hex->buf[i]);
		printf("\n");
		// for (int rows = 0; rows < hex->len / 16 + 1; )
		break;
		struct const_dec* dec = node->lval;
		iprintf(indent, "DEC %"PRIu64" (0x%016"PRIx64") bytes\n", dec->val, dec->val);
		break;
	case T_REG:
		iprintf(indent, "REG\n");
		dump_ast(indent + 2, node->lval, 1);
		if (node->rval)
			dump_ast(indent + 2, node->rval, 1);
		break;
	case T_REG_SPEC:
		if (first)
			iprintf(indent, "REG_SPEC [%d]\n", node->len);
		dump_ast(indent + 2, node->lval, 1);
		if (node->rval)
			dump_ast(indent, node->rval, 0);
		break;
	case T_DATA_EXPR:
		/* iprintf(indent, "DATA_EXPR\n"); */
		dump_ast(indent, node->lval, 1);
		if (node->rval)
			dump_ast(indent, node->rval, 0);
		break;
	case T_WRITE_EXPR:
		iprintf(indent, "WRITE\n");
		dump_ast(indent + 2, node->lval, 1);
		break;
	case T_READ_EXPR:
		dec = node->lval;
		iprintf(indent, "READ %"PRIu64" (0x%016"PRIx64") bytes\n", dec->val, dec->val);
		break;
	case T_REP_START:
		iprintf(indent, "REP_START\n");
		break;
	case T_STOP:
		iprintf(indent, "STOP\n");
		break;
	case T_MSG:
		if (first)
			iprintf(indent, "MSG [%d]\n", node->len);
		dump_ast(indent + 2, node->lval, 1);
		if (node->rval)
			dump_ast(indent, node->rval, 0);
		break;
	case T_CMD:
		iprintf(indent, "CMD\n");
		if (node->rval)
			dump_ast(indent + 2, node->rval, 1);
		dump_ast(indent + 2, node->lval, 1);
		break;
	case T_CMD_LIST:
		if (first)
			iprintf(indent, "CMD_LIST [%d]\n", node->len);
		dump_ast(indent + 2, node->lval, 1);
		if (node->rval)
			dump_ast(indent, node->rval, 0);
		break;
	default:
		fprintf(stderr, "fatal: unknown node type %d\n", node->type);
		exit(2);
	}
}




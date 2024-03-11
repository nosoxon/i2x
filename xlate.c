#include <inttypes.h>

#include "i2x.h"
#include "ast.h"

#define for_ilist(list, it) \
	for (struct node *__n = (list), *it = __n->lval.n; __n; \
		__n = __n->rval.n, it = __n->lval.n)

struct prog *xlate_prog(struct node *n_cmd_list) {
	struct prog *prog = malloc(sizeof(struct prog));
	prog->len = n_cmd_list->len;
	prog->cmds = calloc(prog->len, sizeof(struct cmd));

	struct node *n = n_cmd_list;
	for (size_t i = 0; n; n = n->rval.n, ++i)
		xlate_cmd(n->lval.n, prog->cmds + i);

	return prog;
}

/* Top level transform */
void xlate_cmd(struct node *n_cmd, struct cmd *cmd) {
	// if (prog->type != T_CMD)
	// 	die("not command");

	cmd->segments = xlate_segments(n_cmd->lval.n, &cmd->len);
	cmd->reg_spec = n_cmd->rval.n ? xlate_reg_spec(n_cmd->rval.n) : NULL;
}

struct msg_segment* xlate_segments(struct node *n_msg_list, size_t *segment_count) {
	size_t nmsg = 0, nseg = 1;
	for (struct node *n = n_msg_list, *prev = NULL; n;
			prev = n, n = n->rval.n) {
		struct node *n_msg = n->lval.n;
		if (n_msg->type == T_WRITE_EXPR && prev && prev->rval.n->type & T_MSG_EXPR)
			++nseg;
		else if (n_msg->type == T_REP_START)
			continue;
		else if (n_msg->type == T_STOP) {
			++nseg;
			continue;
		}

		++nmsg;
	}

	struct msg *msgs = calloc(nmsg, sizeof(struct msg));
	struct msg_segment *segments = calloc(nseg, sizeof(struct msg_segment));

	size_t imsg = 0, iseg = 0, lseg = 0;
	for (struct node *n = n_msg_list; n; n = n->rval.n) {
		struct node *n_msg = n->lval.n;
		if (n_msg->type == T_READ_EXPR) {
			xlate_read_expr(n_msg, msgs + imsg);
			++imsg;
		} else if (n_msg->type == T_WRITE_EXPR) {
			xlate_write_expr(n_msg, msgs + imsg);
			++imsg;
		}

		int stop = n_msg->type & T_MSG_EXPR
			&& n->rval.n && n->rval.n->lval.n->type == T_WRITE_EXPR  // do we need to check rval.n->lval.n != NULL?
			|| n_msg->type == T_STOP || imsg == nmsg;
		if (stop) {
			segments[iseg].msgs = msgs + lseg;
			segments[iseg].len = imsg - lseg;
			++iseg;
			lseg = imsg;
		}
	}

	*segment_count = nseg;
	return segments;
}

void xlate_write_expr(struct node *n_expr, struct msg *msg) {
	size_t nbuf = 0;
	// loop through DATA_EXPRs
	for_ilist(n_expr->lval.n, n_const)
		nbuf += n_const->len; // length of PHEX/DEC in bytes

	uint8_t *buf = malloc(nbuf);
	size_t ibuf = 0;
	// loop through DATA_EXPRs
	for_ilist(n_expr->lval.n, n_const) {
		memcpy(buf + ibuf, n_const->lval.x, n_const->len);
		ibuf += n_const->len;
	}

	msg->flags = 0;
	msg->len = nbuf;
	msg->buf = buf;
}

void xlate_read_expr(struct node *n_expr, struct msg *msg) {
	struct node *dec = n_expr->lval.n;
	msg->flags = I2C_M_RD;
	msg->len = dec->rval.flags.val;
	msg->buf = NULL;
}

void copy_const_val(struct node *n_const, uint8_t *dst) {
	
}

#include <inttypes.h>
#include <linux/i2c.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2x.h"

void die(int code, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	puts("");
	va_end(ap);
	exit(code);
}


/* add 1 with carry and wrap around (unsigned) */
void incn(char* n, int width, int big_endian) {
	for (size_t b = 0; b < width; ++b)
		if (++n[big_endian ? width-1 - b : b]) break;
}

uint8_t *encode_dec(struct const_dec *dec) {
	int big_endian = !!(dec->flags & F_DEC_BIG_ENDIAN);
	int width = dec->flags & F_DEC_WIDTH_MASK;

	if (width < 1)
		die(1, "width must be at least 1");
	if (width > 8)
		die(1, "greater than qword unsupported for now");
	if (dec->val >> 8 * width)
		die(1, "decimal %"PRIu64" wider than %d bytes", width);

	uint8_t *buf = malloc(width);
	for (size_t b = 0; b < width; ++b)
		buf[big_endian ? width-1 - b : b] = (dec->val >> 8*b) & 0xff;

	return buf;
}

void copy_const_val(struct ast_node *ast_const, uint8_t *dst) {
	if (ast_const->type == T_HEX) {
		struct const_hex *hex = ast_const->lval;
		memcpy(dst, hex->buf, ast_const->len);
	} else if (ast_const->type == T_DEC) {
		struct const_dec *dec = ast_const->lval;
		uint8_t *ifmt = encode_dec(dec);
		memcpy(dst, ifmt, ast_const->len);
		free(ifmt);
	}
}

struct reg_spec *xlate_reg_spec(struct ast_node *ast_reg_spec) {
	struct reg_spec *reg_spec = malloc(sizeof(struct reg_spec));

	struct ast_node *ast_reg = ast_reg_spec->lval;
	struct ast_node *lval = ast_reg->lval, *rval;

	reg_spec->len = ast_reg_spec->len;
	reg_spec->width = lval->len;
	if (lval->type == T_DEC) {
		struct const_dec *dec = lval->lval;
		reg_spec->big_endian = !!(dec->flags & F_DEC_BIG_ENDIAN);
	} else if (lval->type == T_HEX)
		reg_spec->big_endian = 1;

	reg_spec->ranges = calloc(reg_spec->len, sizeof(struct reg_range));
	size_t irange = 0;
	for (struct ast_node *p = ast_reg_spec; p; p = p->rval, ++irange) {
		ast_reg = p->lval;
		lval = ast_reg->lval;
		rval = ast_reg->rval;

		if (lval->len != reg_spec->width || rval->len != reg_spec->width)
			die(1, "regspecs must be of explicit equal width");

		reg_spec->ranges[irange].start = malloc(reg_spec->width);
		reg_spec->ranges[irange].stop = malloc(reg_spec->width);
		copy_const_val(lval, reg_spec->ranges[irange].start);
		copy_const_val(rval, reg_spec->ranges[irange].stop);
		incn(reg_spec->ranges[irange].stop,
			reg_spec->width, reg_spec->big_endian);
	}

	return reg_spec;
}

void xlate_write_expr(struct ast_node *ast_write_expr, struct msg *msg) {
	size_t nbuf = 0;
	// loop through DATA_EXPRs
	for (struct ast_node *p = ast_write_expr->lval; p; p = p->rval)
		nbuf += ((struct ast_node *) p->lval)->len; // length of PHEX/DEC in bytes

	uint8_t *buf = malloc(nbuf);
	size_t ibuf = 0;
	// loop through DATA_EXPRs
	for (struct ast_node *p = ast_write_expr->lval; p; p = p->rval) {
		struct ast_node *c = p->lval; // const
		copy_const_val(c, buf + ibuf);
		ibuf += c->len;
	}

	msg->flags = 0;
	msg->len = nbuf;
	msg->buf = buf;
}

void xlate_read_expr(struct ast_node *node, struct msg *msg) {
	struct const_dec *dec = node->lval;
	msg->flags = I2C_M_RD;
	msg->len = dec->val;
	msg->buf = NULL;
}

#define ast_lval(n)	((struct ast_node *) n->lval)
#define ast_rval(n)	((struct ast_node *) n->rval)

struct msg_segment* xlate_segments(struct ast_node *msg_list, size_t *segment_count) {
	size_t nmsg = 0;
	size_t nseg = 1;
	for (struct ast_node *node = msg_list, *prev = NULL; node;
			prev = node, node = node->rval) {
		struct ast_node *ast_msg = node->lval;
		if (ast_msg->type == T_WRITE_EXPR && prev && ast_lval(prev)->type & (T_WRITE_EXPR | T_READ_EXPR))
			++nseg;
		else if (ast_msg->type == T_REP_START)
			continue;
		else if (ast_msg->type == T_STOP) {
			++nseg;
			continue;
		}

		++nmsg;
	}

	struct msg *msgs = calloc(nmsg, sizeof(struct msg));
	struct msg_segment *segments = calloc(nseg, sizeof(struct msg_segment));

	size_t imsg = 0, iseg = 0, lseg = 0;
	for (struct ast_node *node = msg_list; node; node = node->rval) {
		struct ast_node *ast_msg = node->lval;
		if (ast_msg->type == T_READ_EXPR) {
			xlate_read_expr(ast_msg, msgs + imsg);
			++imsg;
		} else if (ast_msg->type == T_WRITE_EXPR) {
			xlate_write_expr(ast_msg, msgs + imsg);
			++imsg;
		}

		int stop = ast_msg->type & (T_WRITE_EXPR | T_READ_EXPR)
			&& node->rval && ast_lval(ast_rval(node))->type == T_WRITE_EXPR
			|| ast_msg->type == T_STOP || imsg == nmsg;
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

void xlate_cmd(struct ast_node *ast_cmd, struct cmd *cmd) {
	// if (prog->type != T_CMD)
	// 	die("not command");

	cmd->segments = xlate_segments(ast_cmd->lval, &cmd->len);
	cmd->reg_spec = ast_cmd->rval ? xlate_reg_spec(ast_cmd->rval) : NULL;
}

struct prog *xlate_prog(struct ast_node* ast_prog) {
	struct prog *prog = malloc(sizeof(struct prog));
	prog->len = ast_prog->len;
	prog->cmds = calloc(prog->len, sizeof(struct cmd));

	size_t icmd = 0;
	for (struct ast_node* n = ast_prog; n; n = n->rval) {
		xlate_cmd(n->lval, prog->cmds + icmd);
		++icmd;
	}

	return prog;
}

// eventually this will create an i2c_msg
void exec_msg(uint8_t *reg, int width, struct msg *msg) {
	printf("      msg ");
	if (msg->flags & I2C_M_RD) {
		if (width) {
			printf("[??:W");
			for (size_t i = 0; i < width; ++i)
				printf(" %02hhx", reg[i]);
			printf("] ");
		}
		printf("[??:R%d", msg->len);
	} else {
		printf("[??:W");
		for (size_t i = 0; i < width; ++i)
			printf(" %02hhx", reg[i]);
		for (size_t i = 0; i < msg->len; ++i)
			printf(" %02hhx", msg->buf[i]);
	}
	printf("]\n");
}

void exec_segments(uint8_t *reg, int width, struct cmd *cmd) {
	for (size_t i = 0; i < cmd->len; ++i) {
		struct msg_segment *seg = cmd->segments + i;
		printf("    segment [");
		for (size_t _i = 0; _i < width; ++_i)
			printf("%02hhx", reg[_i]);
		printf("] (%d)\n", seg->len);
		for (size_t j = 0; j < seg->len; ++j)
			exec_msg(reg, width, seg->msgs + j);
	}
}

void iterate_reg_spec(struct cmd *cmd) {
	struct reg_spec *spec = cmd->reg_spec;
	char* reg = malloc(spec->width);
	for (size_t i = 0; i < spec->len; ++i) {
		memcpy(reg, spec->ranges[i].start, spec->width);
		do {
			printf("    ---\n");
			exec_segments(reg, spec->width, cmd);
			incn(reg, spec->width, spec->big_endian);
		} while(memcmp(reg, spec->ranges[i].stop, spec->width));
	}
	free(reg);
}


void exec_cmd(struct cmd *cmd) {
	printf("  cmd [%d]\n", cmd->len);
	if (cmd->reg_spec)
		iterate_reg_spec(cmd);
	else
		exec_segments(NULL, 0, cmd);
}

void exec_prog(struct prog *prog) {
	printf("prog [%d]\n", prog->len);
	for (size_t i = 0; i < prog->len; ++i)
		exec_cmd(prog->cmds + i);
}


void free_ast(struct ast_node *node) {
	if (node->type == T_HEX) {
		struct const_hex *hex = node->lval;
		free(hex->buf);
		free(hex);
	} else if (node->type == T_DEC) {
		free(node->lval);
	} else {
		if (node->lval) free_ast(node->lval);
		if (node->rval) free_ast(node->rval);
	}

	free(node);
}

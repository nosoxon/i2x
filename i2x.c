#include <assert.h>
#include <err.h>
#include <inttypes.h>
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
		if (++n[big_endian ? width-1 - b : b])
			break;
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
		if (msg->flags & I2X_MSG_RD) {
			++nrd;
			/*
			 * If a register address write is prepended, then
			 * add a stop to the previous message, unless it is
			 * explicitly marked as a repeated start.
			 */
			if (reg_spec && msg_prev &&
					!(msg_prev->flags & I2X_MSG_SR)) {
				msg_prev->flags |= I2X_MSG_STOP;
				++nseg;
			}
		}
		if (msg->flags & I2X_MSG_STOP)
			++nseg;
		if (msg->flags & I2X_MSG_PAUSE)
			++nseg, ++npause;
	}

	/* an additional Wr for each Rd if it is prefixed by register */
	size_t n = msg_list->len + nrd - npause;
	struct i2c_msg *kmsgs = calloc(n, sizeof(struct i2c_msg));
	uint16_t *msgflags = calloc(n, sizeof(uint16_t));
	assert(kmsgs && msgflags);

	size_t next_segment = 0, k = 0;
	i2x_list_foreach (i2x_msg, msg, msg_list) {
		if (msg->flags & I2X_MSG_PAUSE) {
			i2x_list_get(i2x_segment, segment_list,
			             segment_list->len - 1)->delay = msg->len;
			continue;
		}

		/* insert write for register pointer if read (2 msg total) */
		if (reg_spec && msg->flags & I2X_MSG_RD) {
			kmsgs[k].addr = 0;
			kmsgs[k].len = reg_width;
			kmsgs[k].buf = malloc(reg_width);
			assert(kmsgs[k].buf);
			msgflags[k] |= I2X_MSG_REG;
			++k;
		}

		kmsgs[k].addr = 0;
		kmsgs[k].flags = msg->flags & I2X_MSG_RD ? I2C_M_RD : 0;
		kmsgs[k].len = msg->len;
		kmsgs[k].buf = msg->buf;
		msgflags[k] = msg->flags;

		/* prepend to msg buffer if write (1 msg total) */
		if (reg_spec && !(msg->flags & I2X_MSG_RD)) {
			uint8_t *buf = malloc(reg_width + msg->len);
			assert(buf);

			memcpy(buf + reg_width, msg->buf, msg->len);
			kmsgs[k].len += reg_width;
			kmsgs[k].buf = buf;
			free(msg->buf);
			msgflags[k] |= I2X_MSG_REG;
		}

		if (msg->flags & I2X_MSG_STOP) {
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
	i2x_list_get(i2x_msg, msg_list, msg_list->len - 1)->flags |= I2X_MSG_STOP;

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
	segment->delay = 0;

	return segment;
}

struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len, uint16_t flags)
{
	assert(flags & I2X_MSG_PAUSE || len);
	struct i2x_msg *msg = malloc(sizeof(struct i2x_msg));
	assert(msg);

	msg->buf = buf;
	msg->len = len;
	msg->flags = flags;

	if (flags & I2X_MSG_RD) {
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

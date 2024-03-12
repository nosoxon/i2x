#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "i2x.h"

struct i2x_list *i2x_cmd_segment(struct i2x_list *msg_list,
				struct i2x_list *reg_spec) {
	struct i2x_list *segment_list = i2x_list_make();

	size_t width = 0;
	if (reg_spec) {
		width = ((struct i2x_regrange *) i2x_list_get(reg_spec, 0))->width;
		assert(width);
	}

	size_t nrd = 0, nseg = 0;
	for (size_t i = 0; i < msg_list->len; ++i) {
		struct i2x_msg *msg = i2x_list_get(msg_list, i);
		if (msg->flags & F_MSG_RD)
			++nrd;
		if (msg->flags & F_MSG_STOP)
			++nseg;
	}

	/* an additional i2c_msg (W) for each read if it is prefixed by register*/
	size_t n = msg_list->len + nrd;
	struct i2c_msg *k_msgs = calloc(n, sizeof(struct i2c_msg));
	assert(k_msgs);

	size_t lseg = 0;
	for (size_t i = 0, j = 0; i < msg_list->len; ++i, ++j) {
		struct i2x_msg *msg = i2x_list_get(msg_list, i);
		if (reg_spec && msg->flags & F_MSG_RD) {
			/* insert write for register pointer */
			k_msgs[j].addr = 0;
			k_msgs[j].flags = F_MSG_REG;
			k_msgs[j].len = width;
			k_msgs[j].buf = malloc(width);
			assert(k_msgs[j].buf);
			++j;
		}

		k_msgs[j].addr = 0;
		k_msgs[j].flags = msg->flags & F_MSG_RD ? I2C_M_RD : 0;
		k_msgs[j].len = msg->len;
		k_msgs[j].buf = msg->buf;

		if (reg_spec && !(msg->flags & F_MSG_RD)) {
			uint8_t *buf = malloc(width + msg->len);
			assert(buf);

			memcpy(buf + width, msg->buf, msg->len);
			k_msgs[j].flags |= F_MSG_REG;
			k_msgs[j].len += width;
			k_msgs[j].buf = buf;
			free(msg->buf);
		}

		if (msg->flags & F_MSG_STOP) {
			struct i2x_segment *segment =
				i2x_segment_make(k_msgs + lseg, j + 1 - lseg);
			i2x_list_extend(segment_list, segment);
			lseg = j + 1;
		}
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

	// last message in msg_list necessarily has a stop
	((struct i2x_msg *) i2x_list_get(msg_list, msg_list->len - 1))
		->flags |= F_MSG_STOP;

	cmd->segment_list = i2x_cmd_segment(msg_list, reg_spec);
	cmd->reg_spec = reg_spec;

	return cmd;
}

struct i2x_segment *i2x_segment_make(struct i2c_msg *msgs, int nmsgs)
{
	struct i2x_segment *segment = malloc(sizeof(struct i2x_segment));
	assert(segment);

	segment->msgset.msgs = msgs;
	segment->msgset.nmsgs = nmsgs;

	return segment;
}

struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len)
{
	assert(len);
	struct i2x_msg *msg = malloc(sizeof(struct i2x_msg));
	assert(msg);

	msg->buf = buf;
	msg->len = len;
	msg->flags = 0;

	if (!buf) { /* read operation */
		msg->buf = malloc(len);
		assert(msg->buf);
		msg->flags |= F_MSG_RD;
	}
	return msg;
}

struct i2x_regrange *i2x_regrange_make(struct i2x_literal *lower,
					struct i2x_literal *upper)
{
	assert(lower && upper);
	assert(lower->len == upper->len);
	struct i2x_regrange *regrange = malloc(sizeof(struct i2x_regrange));
	assert(regrange);

	regrange->lower = lower->buf;
	regrange->upper = upper->buf;
	regrange->width = lower->len;
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

void *i2x_list_get(struct i2x_list *list, size_t index)
{
	assert(list && index < list->len);
	return list->array[index];
}

void i2x_list_free(struct i2x_list *list)
{
	assert(list);
	for (size_t i = 0; i < list->len; ++i)
		free(i2x_list_get(list, i));
	free(list);
}

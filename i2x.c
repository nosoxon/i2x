#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

#include "i2x.h"

struct i2x_cmd *i2x_cmd_make(struct i2x_list *msg_list,
				struct i2x_list *reg_spec)
{
	assert(msg_list);
	struct i2x_cmd *cmd = malloc(sizeof(struct i2x_cmd));
	assert(cmd);

	cmd->msg_list = msg_list;
	cmd->reg_spec = reg_spec;
	return cmd;
}

struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len)
{
	assert(len);
	struct i2x_msg *msg = malloc(sizeof(struct i2x_msg));
	assert(msg);

	msg->buf = buf;
	msg->len = len;
	msg->flags = buf ? 0 : F_MSG_RD;
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
	regrange->len = lower->len;
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

struct i2x_list *i2x_list_make(void *element)
{
	struct i2x_list *list = malloc(sizeof(struct i2x_list));
	assert(list);

	list->array = malloc(LIST_INIT_CAP * sizeof(void *));
	assert(list->array);

	list->array[0] = element;
	list->len = 1;
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

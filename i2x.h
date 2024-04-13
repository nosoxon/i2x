#pragma once

#include <assert.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stddef.h>
#include <stdint.h>

#include "i2x.tab.h"

#define	LIST_INIT_CAP	2

struct i2x_list {
	void				**array;
	size_t				len;
	size_t				cap;
};

struct i2x_target {
	uint32_t			bus;
	/* TODO support 10-bit */
	uint8_t				address;
};

struct i2x_literal {
	uint8_t				*buf;
	size_t				len;
	uint64_t			val;
	int				type;
#define		I2X_LIT_DEC_LE		0b00
#define		I2X_LIT_DEC_BE		0b01
#define		I2X_LIT_HEX_LE		0b10
#define		I2X_LIT_HEX_BE		0b11
};

struct i2x_regrange {
	uint8_t				*start;
	uint8_t				*stop;
	size_t				width;
	int				big_endian;
};

struct i2x_msg {
	uint8_t				*buf;
	size_t				len;
	uint16_t			flags;
#define 	I2X_MSG_RD		(1 << 0)
#define 	I2X_MSG_PAUSE		(1 << 1)
#define 	I2X_MSG_STOP		(1 << 8)
#define 	I2X_MSG_SR		(1 << 9)
/* set on any message that should have register filled in */
#define 	I2X_MSG_REG		(1 << 10)
};

struct i2x_segment {
	struct i2c_rdwr_ioctl_data	msgset;
	uint16_t			*msgflags;
	uint32_t			delay;
};

struct i2x_cmd {
	struct i2x_list			*reg_spec;
	struct i2x_list			*msg_list;
	struct i2x_list			*segment_list;
};

struct i2x_prog {
	struct i2x_target		*target;
	struct i2x_list			*cmd_list;

	int				verbose;
	int				dry_run;
	int				tree;
	enum {
		HEX, PLAIN, DEC, RAW
	}				output_format;	
};

struct i2x_cmd *i2x_cmd_make(struct i2x_list *msg_list,
			     struct i2x_list *reg_spec);
struct i2x_segment *i2x_segment_make(struct i2c_msg *msgs, uint16_t *msgflags,
				     int nmsgs);
struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len, uint16_t flags);
struct i2x_regrange *i2x_regrange_make(struct i2x_literal *lower,
				       struct i2x_literal *upper);
struct i2x_literal *i2x_literal_extend(struct i2x_literal *dst,
				       struct i2x_literal *src);

struct i2x_list *i2x_list_make();
struct i2x_list *i2x_list_extend(struct i2x_list *list, void *element);
void i2x_list_free(struct i2x_list *list);

#define i2x_list_get(type, list, index) ({		\
	assert((list) && (index) < (list)->len);	\
	(struct type *) (list)->array[index]; })

#define i2x_list_foreach(type, it, list)				\
	for (struct type **_it = (struct type **) (list)->array,	\
	*it = *_it, __attribute__((unused)) *it##_prev = NULL;		\
	_it < (struct type **) (list)->array + (list)->len;		\
	it##_prev = *_it, it = *++_it)

void i2x_exec_prog(struct i2x_prog *prog);

void yyerror(void *scanner, void *prog, char *s);

/* add 1 with carry and wrap around (unsigned) */
void incn(uint8_t *n, int width, int big_endian);

void i2x_dump_tree(struct i2x_prog *prog);

void i2x_exec_prog(struct i2x_prog *prog);

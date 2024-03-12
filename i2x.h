#include <stddef.h>
#include <stdint.h>

#define	LIST_INIT_CAP	2

struct i2x_list {
	void	**array;
	size_t	len;
	size_t	cap;
};

struct i2x_literal {
	uint8_t		*buf;
	size_t		len;
	uint64_t	val;
};

struct i2x_regrange {
	uint8_t		*lower;
	uint8_t		*upper;
	size_t		len;
};

struct i2x_msg {
	uint8_t		*buf;
	size_t		len;
	uint16_t	flags;
#define F_MSG_RD	(1 << 0)
#define F_MSG_STOP	(1 << 8)
};

struct i2x_cmd {
	struct i2x_list	*msg_list;
	struct i2x_list	*reg_spec;
};

struct i2x_cmd *i2x_cmd_make(struct i2x_list *msg_list,
				struct i2x_list *reg_spec);
struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len);
struct i2x_regrange *i2x_regrange_make(struct i2x_literal *lower,
					struct i2x_literal *upper);
struct i2x_literal *i2x_literal_extend(struct i2x_literal *dst,
					struct i2x_literal *src);

struct i2x_list *i2x_list_make(void *element);
struct i2x_list *i2x_list_extend(struct i2x_list *list, void *element);
void *i2x_list_get(struct i2x_list *list, size_t index);

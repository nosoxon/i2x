#include <linux/i2c.h>
#include <linux/i2c-dev.h>
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
	size_t		width;
};

struct i2x_msg {
	uint8_t		*buf;
	size_t		len;
	uint16_t	flags;
#define F_MSG_RD	(1 << 0)
#define F_MSG_STOP	(1 << 8)
#define F_MSG_REG	(1 << 9)
};

struct i2x_segment {
	struct i2c_rdwr_ioctl_data	msgset;
	uint32_t			flags;
	struct timespec			*delay;
};

struct i2x_cmd {
	struct i2x_list	*reg_spec;
	struct i2x_list	*msg_list;
	struct i2x_list	*segment_list;
};

struct i2x_cmd *i2x_cmd_make(struct i2x_list *msg_list,
				struct i2x_list *reg_spec);
struct i2x_segment *i2x_segment_make(struct i2c_msg *msgs, int nmsgs);
struct i2x_msg *i2x_msg_make(uint8_t *buf, size_t len);
struct i2x_regrange *i2x_regrange_make(struct i2x_literal *lower,
					struct i2x_literal *upper);
struct i2x_literal *i2x_literal_extend(struct i2x_literal *dst,
					struct i2x_literal *src);

struct i2x_list *i2x_list_make();
struct i2x_list *i2x_list_extend(struct i2x_list *list, void *element);
void *i2x_list_get(struct i2x_list *list, size_t index);
void i2x_list_free(struct i2x_list *list);

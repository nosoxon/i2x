#include <stddef.h>
#include <stdint.h>

struct const_dec {
	/*              WWWE */
	uint8_t		flags;
#define F_DEC_BIG_ENDIAN	(1 << 31)
#define F_DEC_WIDTH_MASK	(~F_DEC_BIG_ENDIAN)
	uint64_t 	val;
};

struct const_hex {
	size_t		len;
	char		*buf;
};

struct ast_node {
	int	type;
#define T_EMPTY		(1 << 0)
#define T_HEX		(1 << 1)
#define T_DEC		(1 << 2)
#define T_REG		(1 << 3)
#define T_REG_SPEC	(1 << 5)
#define T_DATA_EXPR	(1 << 6)
#define	T_WRITE_EXPR	(1 << 7)
#define T_READ_EXPR	(1 << 8)
#define T_REP_START	(1 << 9)
#define T_STOP		(1 << 10)
#define T_MSG		(1 << 11)
#define T_CMD		(1 << 12)
#define T_CMD_LIST	(1 << 13)
	void	*lval;
	void	*rval;
	size_t	len;
};

/*                   */

struct prog {
	size_t			len;
	struct cmd		*cmds;
};

struct cmd {
	struct reg_spec		*reg_spec;
	size_t			len;
	struct msg_segment	*segments;
};

struct reg_spec {
	int			big_endian;
	// what's the max width??
	int			width;
	size_t			len;
	struct reg_range	*ranges;
};

struct reg_range {
	uint8_t			*start;
	uint8_t			*stop;
};

struct msg_segment {
	size_t			len;
	struct msg		*msgs;
};

struct msg {
	uint16_t		flags;
	size_t			len;
	uint8_t			*buf;
};

uint8_t *encode_dec(struct const_dec *dec);
struct prog *xlate_prog(struct ast_node* ast_prog);
void exec_prog(struct prog *prog);

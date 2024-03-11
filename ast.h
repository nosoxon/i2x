#include <stddef.h>
#include <stdint.h>

union leaf {
	struct node	*n;
	uint64_t	*i;
	char		*x;
	struct {
		uint32_t big_endian	: 1;
		uint32_t val		: 31;
	}		flags;
#define F_DEC_BIG_ENDIAN	(1 << 31)
#define F_DEC_WIDTH_MASK	(~F_DEC_BIG_ENDIAN)
};

struct node {
	uint16_t	type;
#define T_HEX		(1 << 0)
#define T_DEC		(1 << 1)
#define T_CONST		(T_HEX | T_DEC)
#define T_REG		(1 << 2)
#define T_REG_SPEC	(1 << 3)
#define T_DATA_EXPR	(1 << 4)
#define	T_WRITE_EXPR	(1 << 5)
#define T_READ_EXPR	(1 << 6)
#define T_MSG_EXPR	(T_WRITE_EXPR | T_READ_EXPR)
#define T_REP_START	(1 << 7)
#define T_STOP		(1 << 8)
#define T_MSG		(1 << 9)
#define T_CMD		(1 << 10)
#define T_CMD_LIST	(1 << 11)
	union leaf	lval, rval;
	size_t		len;
};


D		[0-9]
H		[a-fA-F0-9]
IS		[iI]
DEC		0|[1-9]{D}*

%{
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include "i2x.h"
#include "i2x.tab.h"

void parse_target();
int parse_plain_hex();
void parse_decimal();

static int next_tok_dec = 0;
%}

%option reentrant bison-bridge
%option noyywrap nounput noinput
/*%option noyywrap nounput noinput nodefault*/
/*%option noyyalloc noyyrealloc noyyfree*/

%%

"w"			return WRITE;
"r"			{
	next_tok_dec = 1;
	return READ;
}
"t"			{
	next_tok_dec = 1;
	return PAUSE;
}

[1-9]?{D}:{H}{2}	{
	parse_target(yyscanner);
	return TARGET;
}

{H}+			{
	if (parse_plain_hex(yyscanner))
		return PHEX;
	REJECT
}

([1-8]?{IS})?{DEC}	{
	parse_decimal(yyscanner);
	return DECIMAL;
}

"."			return '.';
","			return ',';
"/"			return '/';
"-"			return '-';

[ \t\v\n\f]		/* snarf */
.			errx(1, "unexpected token encountered");

%%

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>

uint8_t *encode_dec(uint64_t val, int width, int big_endian);

int parse_plain_hex(yyscan_t scanner)
{
	if (next_tok_dec || yyget_leng(scanner) % 2) {
		if (!next_tok_dec && yyget_leng(scanner) > 2)
			warnx("warning: literal `%s' will not be parsed as hex",
			      yyget_text(scanner));
		return 0;
	}

	size_t len = yyget_leng(scanner) / 2;
	uint8_t *buf = malloc(len);
	assert(buf);

	for (size_t i = 0; i < len; ++i)
		sscanf(yyget_text(scanner) + 2*i, "%2hhx", buf + i);

	struct i2x_literal *literal = malloc(sizeof(struct i2x_literal));
	literal->buf = buf;
	literal->len = len;
	literal->val = -1;
	literal->type = I2X_LIT_HEX_BE;

	(*yyget_lval(scanner)).literal = literal;
	return 1;
}

void parse_decimal(yyscan_t scanner)
{
	char *text = yyget_text(scanner);
	uint8_t width = 1;
	int big_endian = 0; /* default LE for now */

	size_t pos = 0;
	if ((text[1] | 32) == 'i') {/* w[dD] (case is bit 5)*/
		width = text[0] - '0';
		pos = 2;
	} else if ((text[0] | 32) == 'i') {
		pos = 1;
	}

	if (pos > 0)
		big_endian = !(text[pos - 1] & 32);

	/* TODO check for error */
	uint64_t val = strtoull(text + pos, NULL, 10);

	struct i2x_literal *literal = malloc(sizeof(struct i2x_literal));
	literal->buf = next_tok_dec ? NULL : encode_dec(val, width, big_endian);
	literal->len = width;
	literal->val = val;
	literal->type = big_endian ? I2X_LIT_DEC_BE : I2X_LIT_DEC_LE;

	(*yyget_lval(scanner)).literal = literal;
	next_tok_dec = 0;
}

void parse_target(yyscan_t scanner)
{
	/* TODO should I be worried about the garbage in structures w padding?*/
	struct i2x_target *target = malloc(sizeof(struct i2x_target));
	assert(target);
	/* TODO validate */
	sscanf(yyget_text(scanner), "%"SCNu32":%"SCNx8,
	       &target->bus, &target->address);

	(*yyget_lval(scanner)).target = target;
}

uint8_t *encode_dec(uint64_t val, int width, int big_endian)
{
	if (width < 1)
		errx(1, "width must be at least 1");
	if (width > 8)
		errx(1, "greater than qword unsupported for now");
	if (val >> 8 * width)
		errx(1, "decimal %"PRIu64" wider than %d bytes", val, width);

	uint8_t *buf = malloc(width);
	for (size_t b = 0; b < width; ++b)
		buf[big_endian ? width-1 - b : b] = (val >> 8 * b) & 0xff;

	return buf;
}

void yyerror(yyscan_t scanner, void **prog, char *s)
{
	fprintf(stderr, "error: %s\n", s);
}

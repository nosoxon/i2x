D		[0-9]
H		[a-fA-F0-9]
IS		[iI]
DEC		[1-9]{D}*

%{
#include <stdio.h>
#include <stdlib.h>

#include "i2x.h"
#include "i2x.tab.h"

int parse_plain_hex();
void parse_decimal();

static int read_encountered = 0;
%}

%option noyywrap nounput noinput
/*%option noyywrap nounput noinput nodefault*/
/*%option noyyalloc noyyrealloc noyyfree*/

%%

"r"			{
	read_encountered = 1;
	return READ;
}
"w"			return WRITE;

{H}+			{
	if (parse_plain_hex())
		return PHEX;
	REJECT
}

([1-8]?{IS})?{DEC}	{
	parse_decimal();
	return DECIMAL;
}

":"			return ':';
"."			return '.';
","			return ',';
"/"			return '/';
"-"			return '-';

[ \t\v\n\f]	/* snarf */
.		{
	fprintf(stderr, "failure");
	exit(1);
}

%%

#include <assert.h>
#include <stdarg.h>

uint8_t *encode_dec(uint64_t val, int width, int big_endian);

int parse_plain_hex() {
	if (read_encountered || yyleng % 2) {
		if (!read_encountered && yyleng > 2)
			fprintf(stderr, "warning: literal `%s' will not be parsed as hex", yytext);
		read_encountered = 0;
		return 0;
	}

	size_t len = yyleng / 2;
	uint8_t *buf = malloc(len);
	assert(buf);

	for (size_t i = 0; i < len; ++i)
		sscanf(yytext + 2*i, "%2hhx", buf + i);

	struct i2x_literal *literal = malloc(sizeof(struct i2x_literal));
	literal->buf = buf;
	literal->len = len;
	literal->val = -1;
	literal->type = I2X_LIT_HEX_BE;

	yylval.literal = literal;
	return 1;
}

void parse_decimal() {
	uint8_t width = 1;
	int big_endian = 0; /* default LE for now */

	size_t pos = 0;

	if ((yytext[1] | 32) == 'i') {/* w[dD] (case is bit 5)*/
		width = yytext[0] - '0';
		pos = 2;
	} else if ((yytext[0] | 32) == 'i') {
		pos = 1;
	}

	if (pos > 0)
		big_endian = !(yytext[pos - 1] & 32);

	/* TODO check for error */
	uint64_t val = strtoull(yytext + pos, NULL, 10);

	struct i2x_literal *literal = malloc(sizeof(struct i2x_literal));
	literal->buf = encode_dec(val, width, big_endian);
	literal->len = width;
	literal->val = val;
	literal->type = big_endian ? I2X_LIT_DEC_BE : I2X_LIT_DEC_LE;

	yylval.literal = literal;
}

void die(int code, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	puts("");
	va_end(ap);
	exit(code);
}

uint8_t *encode_dec(uint64_t val, int width, int big_endian) {
	if (width < 1)
		die(1, "width must be at least 1");
	if (width > 8)
		die(1, "greater than qword unsupported for now");
	if (val >> 8 * width)
		die(1, "decimal %"PRIu64" wider than %d bytes", width);

	uint8_t *buf = malloc(width);
	for (size_t b = 0; b < width; ++b)
		buf[big_endian ? width-1 - b : b] = (val >> 8 * b) & 0xff;

	return buf;
}


void yyerror(char *s) {
	fprintf(stderr, "error: %s\n", s);
}

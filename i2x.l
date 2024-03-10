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



int parse_plain_hex() {
	if (read_encountered || yyleng % 2) {
		read_encountered = 0;
		/* fprintf(stderr, "len must be mult 2 %s[%d]\n", yytext, yyleng); */
		return 0;
		//exit(2);
	}

	size_t len = yyleng / 2;
	char *buf = malloc(len);
	// if (err) ...

	for (size_t i = 0; i < len; ++i)
		sscanf(yytext + 2*i, "%2hhx", buf + i);

	struct const_hex* v = malloc(sizeof(struct const_hex));
	v->len = len;
	v->buf = buf;

	yylval.hex = v;
	return 1;
}

void parse_decimal() {
	uint8_t width = 1;
	int little_endian = 1; /* default LE for now */

	size_t pos = 0;

	if ((yytext[1] | 32) == 'i') {/* w[dD] (case is bit 5)*/
		width = yytext[0] - '0';
		pos = 2;
	} else if ((yytext[0] | 32) == 'i')
		pos = 1;

	if (pos > 0)
		little_endian = !!(yytext[pos - 1] & 32);
	

	int host_endian = (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);

	uint64_t val = strtoull(yytext + pos, NULL, 10);
	/* fprintf(stderr, "yyleng=%d,yytext+pos=%s,val=%"PRIu64"\n", yyleng, yytext + pos, val); */
	/* if (width > 1 && little_endian ^ host_endian) {
		if ()
		switch (width) {
		case 2:

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
			if ()
#else
#endif
		}
		if (little_endian)
			htonl(val)
	} */

	// endptr check
	/* if (val > 255) {
		fprintf(stderr, "please use a width specifier\n");
		exit(1);
	} */

	struct const_dec* v = malloc(sizeof(struct const_dec));
	v->flags =
		(little_endian ? 0 : F_DEC_BIG_ENDIAN) | width;
	v->val = val;

	yylval.dec = v;
}
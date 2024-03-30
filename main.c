#include <assert.h>
#include <getopt.h>
#include <string.h>

#include "dump.h"
#include "i2x.tab.h"
#include "i2x.yy.h"

static const struct option longopts[] = {
	{ "hex",   no_argument, NULL, 'x' },
	{ "dec",   no_argument, NULL, 'd' },
	{ "raw",   no_argument, NULL, 'r' },
	{ "plain", no_argument, NULL, 'p' },
};

#define F_FMT_HEX	1
#define F_FMT_DEC	2
#define F_FMT_RAW	3
#define F_FMT_PLAIN	4

int main(int argc, char **argv)
{
	int output_format = F_FMT_HEX;

	char c;
	while ((c = getopt_long(argc, argv, "+xdrp", longopts, NULL) != -1)) {
		switch (c) {
		case 'x':
			output_format = F_FMT_HEX;
			break;
		case 'd':
			output_format = F_FMT_DEC;
			break;
		case 'r':
			output_format = F_FMT_RAW;
			break;
		case 'p':
			output_format = F_FMT_PLAIN;
			break;
		}
	}

	size_t len = 0;
	for (int i = 1; i < argc; ++i)
		len += strlen(argv[i]) + 1;

	char *raw_input = malloc(len);
	*raw_input = 0;
	assert(raw_input);

	size_t n = 0;
	for (int i = 1; i < argc; ++i) {
		if (i != 1) {
			strcat(raw_input, " ");
			++n;
		}
		strcat(raw_input, argv[i]);
		n += strlen(argv[i]);
	}
	printf("read: `%s'\n", raw_input);

	struct i2x_prog *program;

	yyscan_t scanner;
	yylex_init(&scanner);

	yy_scan_string(raw_input, scanner);
	yyparse(scanner, (void **) &program);

	yylex_destroy(scanner);

	printf("\nDEBUG DUMP\n");
	dump_prog(program);

	printf("\nBEGIN EXECUTE\n");
	i2x_exec_prog(program);
	// yy_delete_buffer(YY_CURRENT_BUFFER);
}


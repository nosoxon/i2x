#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <string.h>

#include "i2x.h"
#include "i2x.tab.h"
#include "i2x.yy.h"

static const struct option longopts[] = {
	{ "verbose", no_argument, NULL, 'v' },
	{ "dry-run", no_argument, NULL, 'n' },
	{ "tree",    no_argument, NULL, 't' },
	{ "hex",     no_argument, NULL, 'x' },
	{ "plain",   no_argument, NULL, 'p' },
	{ "dec",     no_argument, NULL, 'd' },
	{ "raw",     no_argument, NULL, 'r' },
	{ NULL, 0, NULL, 0}
};


int main(int argc, char **argv)
{
	struct i2x_prog *program = malloc(sizeof(struct i2x_prog));
	assert(program);

	program->verbose = 0;
	program->output_format = HEX;

	char c;
	while ((c = getopt_long(argc, argv, "+vntxpdr",
				longopts, NULL)) != -1) {
		switch (c) {
		case 'v':
			++program->verbose;
			break;
		case 'n':
			program->dry_run = 1;
			break;
		case 't':
			program->tree = 1;
			break;
		case 'x':
			program->output_format = HEX;
			break;
		case 'p':
			program->output_format = PLAIN;
			break;
		case 'd':
			program->output_format = DEC;
			break;
		case 'r':
			program->output_format = RAW;
			break;
		case '?':
			/* TODO maybe some more help */
			exit(1);
		}
	}

	/* 
	 * potentially could verbose to stderr, but probably defeats the purpose
	 * of raw output
	 */
	if (program->verbose && program->output_format == RAW)
		errx(1, "raw output precludes verbosity");
	if (program->verbose && program->output_format == PLAIN)
		errx(1, "plain hex output precludes verbosity");
	/* dry run implies verbose */
	if (program->dry_run && !program->verbose)
		program->verbose = 1;

	size_t len = 0;
	for (int i = optind; i < argc; ++i)
		len += strlen(argv[i]) + 1;

	char *raw_input = malloc(len);
	*raw_input = 0;
	assert(raw_input);

	size_t n = 0;
	for (int i = optind; i < argc; ++i) {
		strcat(raw_input, " ");
		strcat(raw_input, argv[i]);
		n += 1 + strlen(argv[i]);
	}

	if (program->verbose)
		printf("read: `%s'\n", raw_input + 1);

	yyscan_t scanner;
	yylex_init(&scanner);

	yy_scan_string(raw_input, scanner);
	int ret = yyparse(scanner, (void *) program);
	printf("yyparse = %d\n", ret);
	if (ret)
		errx(1, "failed to parse program");

	yylex_destroy(scanner);

	if (program->tree)
		i2x_dump_tree(program);
	else
		i2x_exec_prog(program);

	/* free(options); */
	/* yy_delete_buffer(YY_CURRENT_BUFFER); */
}


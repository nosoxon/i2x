#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include "i2x.h"
#include "i2x.tab.h"
#include "i2x.yy.h"

static const char invocation[] = "i2x";

static void __attribute__((__noreturn__)) usage(int eval)
{
	FILE *out = stdout;
	fprintf(out,
		"%1$s - i2c swiss army knife\n"
		"\n"
		"Usage:  %1$s [options] [--] BUS:ADDR CMD_LIST\n"
		"\n"
		"Execute CMD_LIST on i2c bus BUS (decimal), addressing messages to\n"
		"ADDR (hex). CMD_LIST need not be quoted, and spacing within it is\n"
		"mostly optional.\n"
		"\n"
		"Example:\n"
		"\n"
		"        $ %1$s 2:3c 8b w 04 919d7458 t100 / 8b-8e r8\n"
		"\n"
		"        8b: 07 46 cd be 0c 7b 6f cf\n"
		"        8c: 07 9d 74 58 55 0f 17 d1\n"
		"        8d: 07 04 7c ac 03 2a af 3c\n"
		"        8e: 07 06 9a 1b cc 8c ae 91\n"
		"\n"
		"Options:\n"
		"  -d, --dec        pretty print responses in decimal\n"
		"  -x, --hex        pretty print responses in hex (default)\n"
		"  -p, --plain      print responses in plain hex format\n"
		"  -r, --raw        output raw response data\n"
		"\n"
		"  -v, --verbose    show more details\n"
		"  -n, --dry-run    don't dispatch messages, implies --verbose\n"
		"  -t, --tree       just show parse tree\n"
		"  -h, --help       display this help\n", invocation);
	exit(eval);
}

static const struct option longopts[] = {
	{ "dec",     no_argument, NULL, 'd' },
	{ "hex",     no_argument, NULL, 'x' },
	{ "plain",   no_argument, NULL, 'p' },
	{ "raw",     no_argument, NULL, 'r' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "dry-run", no_argument, NULL, 'n' },
	{ "tree",    no_argument, NULL, 't' },
	{ "help",    no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0}
};

int main(int argc, char **argv)
{
	struct i2x_prog *program = malloc(sizeof(struct i2x_prog));
	assert(program);

	program->verbose = 0;
	program->output_format = HEX;

	signed char c;
	while ((c = getopt_long(argc, argv, "+dxprvnth",
				longopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			program->output_format = DEC;
			break;
		case 'x':
			program->output_format = HEX;
			break;
		case 'p':
			program->output_format = PLAIN;
			break;
		case 'r':
			program->output_format = RAW;
			break;
		case 'v':
			++program->verbose;
			break;
		case 'n':
			program->dry_run = 1;
			break;
		case 't':
			program->tree = 1;
			break;
		case 'h':
			usage(0);
		case '?':
			warnx("unrecognized option `%s'", "?");
			usage(1);
		}
	}

	/* dry run implies verbose */
	if (program->dry_run && !program->verbose)
		program->verbose = 1;
	/* 
	 * potentially could verbose to stderr, but probably defeats the purpose
	 * of raw output
	 */
	if (program->verbose && program->output_format == RAW)
		errx(1, "raw output precludes verbosity and dry runs");
	if (program->verbose && program->output_format == PLAIN)
		errx(1, "plain hex output precludes verbosity and dry runs");

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
	if (ret)
		errx(1, "failed to parse program (%d)", ret);

	yylex_destroy(scanner);

	if (program->tree)
		i2x_dump_tree(program);
	else
		i2x_exec_prog(program);

	/* free(options); */
	/* yy_delete_buffer(YY_CURRENT_BUFFER); */
}

#include <assert.h>
#include <string.h>

#include "i2x.tab.h"
#include "i2x.yy.h"


int main(int argc, char **argv) {
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

	yy_scan_string(raw_input);
	yyparse();
	// yy_delete_buffer(YY_CURRENT_BUFFER);
}


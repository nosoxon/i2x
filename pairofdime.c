#include <stdlib.h>
#include <string.h>
#include <stddef.h>

// lists = {
// 	cmds
// 	msgs
// 	regspecs
// }

struct list {
	void	*array;
	size_t	unit;
	size_t	len;
	size_t	cap;
};

void list_append(struct list *list, void *element) {
	assert(list && list->len <= list->cap);
	if (list->len == list->cap) {
		list->cap <<= 1;
		list->array = realloc(list->array, list->cap * list->unit);
		assert(list->array);
	}

	// (ptrdiff_t) 
	memcpy(list->array + list->len * list->unit, element, list->unit);
	++list->len;
}
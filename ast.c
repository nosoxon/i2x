

#include "ast.h"

struct node* ast_node_make(int type, void* lval, void* rval, size_t len) {
	struct node* node = malloc(sizeof(struct node));
	/* TODO alloc check */
	node->type = type;
	node->lval.n = lval;
	node->rval.n = rval;
	node->len = len;
	return node;
}

struct node* ast_list_extend(struct node* head, struct node* val) {
	struct node* prev = head;
	for (; prev->rval.n; prev = prev->rval.n);
	prev->rval.n = ast_node_make(head->type, val, NULL, 1);
	head->len++;
	return head;
}






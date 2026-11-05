#include "vec.h"

#include <stdint.h>
#include <stdlib.h>

int vec_next_cap(size_t old_cap, size_t initial, size_t *new_cap) {
	if (old_cap == 0) {
		*new_cap = initial;
		return 1;
	}

	if (GROWTH_FACTOR == 0 || old_cap > SIZE_MAX / GROWTH_FACTOR) {
		return 0;
	}

	*new_cap = old_cap * GROWTH_FACTOR;
	return 1;
}

void *vec_realloc(void *ptr, size_t count, size_t elem_size) {
	if (elem_size != 0 && count > SIZE_MAX / elem_size) {
		return NULL;
	}

	return realloc(ptr, count * elem_size);
}

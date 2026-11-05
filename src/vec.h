#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>

static const size_t GROWTH_FACTOR = 2;

int vec_next_cap(size_t old_cap, size_t initial, size_t *new_cap);

void *vec_realloc(void *ptr, size_t count, size_t elem_size);

#endif

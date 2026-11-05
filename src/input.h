#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>
#include <stdio.h>

typedef enum {
	INPUT_OK,
	INPUT_EOF,
	INPUT_ERR_IO,
	INPUT_ERR_MEMORY,
	INPUT_ERR_INVALID,
} InputResult;

typedef struct {
	FILE  *stream;
	char  *line;
	size_t length;
	size_t capacity;
	size_t line_number;
} Input;

void input_init(Input *input, FILE *stream);

InputResult input_read_line(Input *input);

int input_parse_counts(const char *line, size_t *values, size_t count, size_t max);

void input_free(Input *input);

#endif

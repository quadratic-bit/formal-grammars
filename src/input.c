#include "input.h"

#include "vec.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static const size_t DEFAULT_LINE_CAP = 64;

void input_init(Input *input, FILE *stream) {
	assert(input  != NULL);
	assert(stream != NULL);

	*input = (Input){.stream = stream};
}

static int input_reserve(Input *input, size_t needed) {
	if (needed <= input->capacity) {
		return 1;
	}

	size_t cap = input->capacity;
	while (cap < needed) {
		if (!vec_next_cap(cap, DEFAULT_LINE_CAP, &cap)) {
			return 0;
		}
	}

	char *line = vec_realloc(input->line, cap, sizeof *line);
	if (line == NULL) {
		return 0;
	}

	input->line     = line;
	input->capacity = cap;
	return 1;
}

InputResult input_read_line(Input *input) {
	assert(input         != NULL);
	assert(input->stream != NULL);

	input->length = 0;

	for (;;) {
		int c = fgetc(input->stream);
		if (c == EOF) {
			if (ferror(input->stream)) {
				return INPUT_ERR_IO;
			}
			if (input->length == 0) {
				return INPUT_EOF;
			}
			break;
		}
		if (c == '\n') {
			break;
		}
		if (c == '\0') {
			return INPUT_ERR_INVALID;
		}
		if (input->length > SIZE_MAX - 2 || !input_reserve(input, input->length + 2)) {
			return INPUT_ERR_MEMORY;
		}
		input->line[input->length++] = (char)c;
	}

	if (input->length > 0 && input->line[input->length - 1] == '\r') {
		input->length--;
	}

	if (!input_reserve(input, input->length + 1)) {
		return INPUT_ERR_MEMORY;
	}
	input->line[input->length] = '\0';
	input->line_number++;
	return INPUT_OK;
}

int input_parse_counts(const char *line, size_t *values, size_t count, size_t max) {
	assert(line   != NULL);
	assert(values != NULL || count == 0);

	const unsigned char *p = (const unsigned char *)line;
	for (size_t i = 0; i < count; ++i) {
		while (isspace(*p)) {
			p++;
		}
		if (*p < '0' || *p > '9') {
			return 0;
		}

		size_t value = 0;
		while (*p >= '0' && *p <= '9') {
			size_t digit = (size_t)(*p - '0');
			if (value > max / 10 || (value == max / 10 && digit > max % 10)) {
				return 0;
			}
			value = value * 10 + digit;
			p++;
		}
		values[i] = value;
		if (*p != '\0' && !isspace(*p)) {
			return 0;
		}
	}

	while (isspace(*p)) {
		p++;
	}
	return *p == '\0';
}

void input_free(Input *input) {
	assert(input != NULL);
	free(input->line);
	*input = (Input){0};
}

#include "../src/input.h"

#include <criterion/criterion.h>

#include <stdio.h>
#include <string.h>

Test(input, preserves_empty_lines_and_crlf) {
	FILE *file = tmpfile();
	cr_assert_not_null(file);

	const char *source = "a\n\nb\r\nlast";

	cr_assert_eq(fwrite(source, 1, strlen(source), file), strlen(source));

	rewind(file);

	Input input;
	input_init(&input, file);

	cr_assert_eq    (input_read_line(&input), INPUT_OK);
	cr_assert_str_eq(input.line, "a");
	cr_assert_eq    (input.length, 1);

	cr_assert_eq    (input_read_line(&input), INPUT_OK);
	cr_assert_str_eq(input.line, "");
	cr_assert_eq    (input.length, 0);

	cr_assert_eq    (input_read_line(&input), INPUT_OK);
	cr_assert_str_eq(input.line, "b");

	cr_assert_eq    (input_read_line(&input), INPUT_OK);
	cr_assert_str_eq(input.line, "last");
	cr_assert_eq    (input.length, 4);

	cr_assert_eq(input_read_line(&input), INPUT_EOF);
	cr_assert_eq(input.line_number, 4);

	input_free(&input);
	fclose(file);
}

Test(input, rejects_embedded_nul) {
	FILE *file = tmpfile();
	cr_assert_not_null(file);

	const unsigned char source[] = {'a', 0, 'b', '\n'};

	cr_assert_eq(fwrite(source, 1, sizeof source, file), sizeof source);

	rewind(file);

	Input input;
	input_init(&input, file);

	cr_assert_eq(input_read_line(&input), INPUT_ERR_INVALID);

	input_free(&input);
	fclose(file);
}

Test(input, parses_counts) {
	size_t values[3] = {0};

	cr_assert(input_parse_counts(" 1 \t 2  3 ", values, 3, 100));

	cr_assert_eq(values[0], 1);
	cr_assert_eq(values[1], 2);
	cr_assert_eq(values[2], 3);
}

Test(input, rejects_invalid_counts) {
	size_t values[3] = {0};

	cr_assert(!input_parse_counts("",        values, 3, 100));
	cr_assert(!input_parse_counts("-1 2 3",  values, 3, 100));
	cr_assert(!input_parse_counts("101 2 3", values, 3, 100));
	cr_assert(!input_parse_counts("1 2",     values, 3, 100));
	cr_assert(!input_parse_counts("1 2 3 4", values, 3, 100));
	cr_assert(!input_parse_counts("1x 2 3",  values, 3, 100));
	cr_assert(!input_parse_counts("1 2 x",   values, 3, 100));
}

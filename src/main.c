#include <grammars/cyk.h>
#include <grammars/grammar.h>
#include <grammars/grammar_parse.h>

#include "input.h"

#include <stddef.h>
#include <stdio.h>

static const size_t MAX_WORDS             = 100000;
static const size_t MAX_TOTAL_WORD_LENGTH = 10000000;

static void print_input_error(InputResult result, size_t line) {
	switch (result) {
	case INPUT_EOF:
		fprintf(stderr, "input error at line %zu: unexpected end of input\n", line);
		break;
	case INPUT_ERR_IO:
		fprintf(stderr, "input error at line %zu: read failed\n", line);
		break;
	case INPUT_ERR_MEMORY:
		fprintf(stderr, "input error at line %zu: out of memory\n", line);
		break;
	case INPUT_ERR_INVALID:
		fprintf(stderr, "input error at line %zu: NUL byte\n", line);
		break;
	case INPUT_OK:
		break;
	}
}

int main(int argc, char **argv) {
	if (argc != 1) {
		fprintf(stderr, "usage: %s < input.txt\n", argv[0]);
		return 1;
	}

	Grammar            grammar = {0};
	GrammarParseError  error   = {0};

	GrammarParseResult parse = grammar_read(&grammar, stdin, &error);
	if (parse != GRAMMAR_PARSE_OK) {
		fprintf(stderr, "grammar error at line %zu: %s\n", error.line, error.message);
		return 1;
	}

	/* header, two symbol lines, all rules, the start line */
	size_t grammar_lines = grammar.rules_count + 4;

	Cyk       cyk    = {0};
	CykResult fitted = cyk_fit(&cyk, &grammar);
	grammar_free(&grammar);

	if (fitted != CYK_OK) {
		fprintf(stderr, "failed to prepare grammar: %s\n",
		        fitted == CYK_ERR_MEMORY ? "out of memory" : "invalid grammar");
		cyk_free(&cyk);
		return 1;
	}

	Input input;
	input_init(&input, stdin);
	input.line_number = grammar_lines;

	InputResult read = input_read_line(&input);
	if (read != INPUT_OK) {
		print_input_error(read, input.line_number + 1);
		goto fail;
	}

	size_t counts[1];
	if (!input_parse_counts(input.line, counts, 1, MAX_WORDS) || counts[0] == 0) {
		fprintf(stderr, "input error: expected word count (1..100000)\n");
		goto fail;
	}

	size_t total_length = 0;
	for (size_t i = 0; i < counts[0]; ++i) {
		read = input_read_line(&input);
		if (read != INPUT_OK) {
			fprintf(stderr, "word %zu: ", i + 1);
			print_input_error(read, input.line_number + 1);
			goto fail;
		}

		if (input.length > MAX_TOTAL_WORD_LENGTH - total_length) {
			fprintf(stderr, "word %zu: total word length exceeds 10000000\n", i + 1);
			goto fail;
		}
		total_length += input.length;

		int accepts = 0;
		CykResult result = cyk_predict(&cyk, input.line, input.length, &accepts);
		if (result != CYK_OK) {
			fprintf(stderr, "word %zu: %s\n", i + 1,
			        result == CYK_ERR_MEMORY ? "out of memory" : "undeclared terminal");
			goto fail;
		}

		if (puts(accepts ? "Yes" : "No") == EOF) {
			fprintf(stderr, "failed to write output\n");
			goto fail;
		}
	}

	read = input_read_line(&input);
	if (read != INPUT_EOF) {
		if (read == INPUT_OK) {
			fprintf(stderr, "input error: unexpected lines after last word\n");
		} else {
			print_input_error(read, input.line_number + 1);
		}
		goto fail;
	}

	input_free(&input);
	cyk_free  (&cyk);
	return 0;

fail:
	input_free(&input);
	cyk_free  (&cyk);
	return 1;
}

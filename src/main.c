#include <grammars/cyk.h>
#include <grammars/earley.h>
#include <grammars/grammar.h>
#include <grammars/grammar_parse.h>

#include "input.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef enum { ALGORITHM_CYK, ALGORITHM_EARLEY } Algorithm;

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
	Algorithm algorithm = ALGORITHM_CYK;

	if (argc > 2) {
		fprintf(stderr, "usage: %s [--cyk|--earley] < input.txt\n", argv[0]);
		return 1;
	}

	if (argc == 2) {
		if (strcmp(argv[1], "--earley") == 0) {
			algorithm = ALGORITHM_EARLEY;
		} else if (strcmp(argv[1], "--cyk") != 0) {
			fprintf(stderr, "usage: %s [--cyk|--earley] < input.txt\n", argv[0]);
			return 1;
		}
	}

	Grammar grammar         = {0};
	GrammarParseError error = {0};
	if (grammar_read(&grammar, stdin, &error) != GRAMMAR_PARSE_OK) {
		fprintf(stderr, "grammar error at line %zu: %s\n", error.line, error.message);
		grammar_free(&grammar);
		return 1;
	}

	size_t grammar_lines = grammar.rules_count + 4;

	Cyk    cyk    = {0};
	Earley earley = {0};
	int fit_ok;

	if (algorithm == ALGORITHM_CYK) {
		CykResult result = cyk_fit(&cyk, &grammar);
		fit_ok = result == CYK_OK;
		if (!fit_ok) {
			fprintf(stderr, "failed to prepare CYK: %s\n", result == CYK_ERR_MEMORY
			                ? "out of memory"
			                : "invalid grammar");
		}
	} else {
		EarleyResult result = earley_fit(&earley, &grammar);
		fit_ok = result == EARLEY_OK;
		if (!fit_ok) {
			fprintf(stderr, "failed to prepare Earley: %s\n",
			                result == EARLEY_ERR_MEMORY
			                ? "out of memory"
			                : "invalid grammar");
		}
	}

	grammar_free(&grammar);
	if (!fit_ok) {
		cyk_free   (&cyk);
		earley_free(&earley);
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
		int memory_error;

		if (algorithm == ALGORITHM_CYK) {
			CykResult result = cyk_predict(&cyk, input.line, input.length, &accepts);
			memory_error = result == CYK_ERR_MEMORY;
			if (result != CYK_OK) {
				fprintf(stderr, "word %zu: %s\n", i + 1, memory_error
				                ? "out of memory"
				                : "undeclared terminal");
				goto fail;
			}
		} else {
			EarleyResult result = earley_predict(&earley, input.line, input.length, &accepts);
			memory_error = result == EARLEY_ERR_MEMORY;
			if (result != EARLEY_OK) {
				fprintf(stderr, "word %zu: %s\n", i + 1, memory_error
				                ? "out of memory"
				                : "undeclared terminal");
				goto fail;
			}
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

	input_free (&input);
	cyk_free   (&cyk);
	earley_free(&earley);
	return 0;

fail:
	input_free (&input);
	cyk_free   (&cyk);
	earley_free(&earley);
	return 1;
}

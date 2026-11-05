#include <grammars/grammar_parse.h>

#include "input.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

static int is_nonterminal(unsigned char c) {
	return c >= 'A' && c <= 'Z';
}

static int is_terminal(unsigned char c) {
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	       c == '(' || c == ')' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%';
}

static GrammarParseResult parse_error(
	GrammarParseError *error,
	GrammarParseResult result,
	size_t      line,
	const char *message
) {
	error->line    = line;
	error->message = message;
	return result;
}

static GrammarParseResult read_required_line(Input *input, GrammarParseError *error) {
	switch (input_read_line(input)) {
	case INPUT_OK:
		return GRAMMAR_PARSE_OK;
	case INPUT_EOF:
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID,
		                   input->line_number + 1, "unexpected end of input");
	case INPUT_ERR_INVALID:
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID,
		                   input->line_number + 1, "NUL byte in input");
	case INPUT_ERR_MEMORY:
		return parse_error(error, GRAMMAR_PARSE_ERR_MEMORY,
		                   input->line_number + 1, "out of memory");
	case INPUT_ERR_IO:
		return parse_error(error, GRAMMAR_PARSE_ERR_IO,
		                   input->line_number + 1, "input read failed");
	}
	return GRAMMAR_PARSE_ERR_IO;
}

static GrammarParseResult add_error(
	GrammarParseError *error,
	GrammarResult      result,
	size_t      line,
	const char *message
) {
	return parse_error(error,
	                   result == GRAMMAR_ERR_MEMORY ? GRAMMAR_PARSE_ERR_MEMORY
	                                                : GRAMMAR_PARSE_ERR_INVALID,
	                   line,
	                   result == GRAMMAR_ERR_MEMORY ? "out of memory" : message);
}

static GrammarParseResult read_symbols(
	Grammar *grammar,
	Input   *input,
	GrammarParseError *error,
	size_t expected,
	int nonterminals
) {
	GrammarParseResult result = read_required_line(input, error);
	if (result != GRAMMAR_PARSE_OK) {
		return result;
	}

	size_t count = 0;
	for (const unsigned char *p = (const unsigned char *)input->line; *p != '\0'; ++p) {
		if (isspace(*p)) {
			continue;
		}
		if (count == expected) {
			return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
			                   "too many declared symbols");
		}
		if (nonterminals ? !is_nonterminal(*p) : !is_terminal(*p)) {
			return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
			                   "invalid declared symbol");
		}
		GrammarResult added = nonterminals
			? grammar_add_nonterminal(grammar, (char)*p)
			: grammar_add_terminal   (grammar, (char)*p);
		if (added != GRAMMAR_OK) {
			return add_error(error, added, input->line_number,
			                 "duplicate or invalid declared symbol");
		}
		count++;
	}

	if (count != expected) {
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
		                   "declared symbol count does not match header");
	}
	return GRAMMAR_PARSE_OK;
}

static GrammarParseResult read_rule(Grammar *grammar, Input *input, GrammarParseError *error) {
	GrammarParseResult result = read_required_line(input, error);
	if (result != GRAMMAR_PARSE_OK) {
		return result;
	}

	char *p = input->line;
	while (isspace((unsigned char)*p)) {
		p++;
	}
	if (!is_nonterminal((unsigned char)*p)) {
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
		                   "rule must begin with a nonterminal");
	}
	char lhs = *p++;
	while (isspace((unsigned char)*p)) {
		p++;
	}
	if (*p != '-' || p[1] != '>') {
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
		                   "rule must contain '->'");
	}
	p += 2;

	char *rhs = p;
	char *end = rhs;
	for (; *p != '\0'; ++p) {
		if (!isspace((unsigned char)*p)) {
			*end++ = *p;
		}
	}
	*end = '\0';

	GrammarResult added = grammar_add_rule(grammar, lhs, rhs);
	if (added != GRAMMAR_OK) {
		return add_error(error, added, input->line_number,
		                 "rule uses an undeclared symbol");
	}
	return GRAMMAR_PARSE_OK;
}

static GrammarParseResult read_start(Grammar *grammar, Input *input, GrammarParseError *error) {
	GrammarParseResult result = read_required_line(input, error);
	if (result != GRAMMAR_PARSE_OK) {
		return result;
	}

	const unsigned char *p = (const unsigned char *)input->line;
	while (isspace(*p)) {
		p++;
	}
	if (!is_nonterminal(*p)) {
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
		                   "invalid start nonterminal");
	}
	char start = (char)*p++;
	while (isspace(*p)) {
		p++;
	}
	if (*p != '\0') {
		return parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input->line_number,
		                   "start line must contain one nonterminal");
	}
	GrammarResult set = grammar_set_start(grammar, start);
	if (set != GRAMMAR_OK) {
		return add_error(error, set, input->line_number,
		                 "undeclared start nonterminal");
	}
	return GRAMMAR_PARSE_OK;
}

GrammarParseResult grammar_read(Grammar *grammar, FILE *stream, GrammarParseError *error) {
	assert(grammar != NULL);
	assert(stream  != NULL);
	assert(error   != NULL);
	assert(grammar->nonterminals_count == 0);
	assert(grammar->terminals_count    == 0);
	assert(grammar->rules_count        == 0);
	assert(grammar->rhs_count          == 0);

	*error = (GrammarParseError){0};
	Grammar parsed = {0};
	Input input;
	input_init(&input, stream);

	GrammarParseResult result = read_required_line(&input, error);
	if (result != GRAMMAR_PARSE_OK) {
		goto fail;
	}

	size_t counts[3];
	if (!input_parse_counts(input.line, counts, 3, 100) || counts[0] == 0) {
		result = parse_error(error, GRAMMAR_PARSE_ERR_INVALID, input.line_number,
		                     "expected nonterminal, terminal and rule counts (0..100)");
		goto fail;
	}

	result = read_symbols(&parsed, &input, error, counts[0], 1);
	if (result != GRAMMAR_PARSE_OK) {
		goto fail;
	}
	result = read_symbols(&parsed, &input, error, counts[1], 0);
	if (result != GRAMMAR_PARSE_OK) {
		goto fail;
	}
	for (size_t i = 0; i < counts[2]; ++i) {
		result = read_rule(&parsed, &input, error);
		if (result != GRAMMAR_PARSE_OK) {
			goto fail;
		}
	}
	result = read_start(&parsed, &input, error);
	if (result != GRAMMAR_PARSE_OK) {
		goto fail;
	}

	*grammar = parsed;
	input_free(&input);
	return GRAMMAR_PARSE_OK;

fail:
	grammar_free(&parsed);
	input_free(&input);
	return result;
}

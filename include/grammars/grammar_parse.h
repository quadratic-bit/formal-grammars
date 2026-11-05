#ifndef GRAMMARS_GRAMMAR_PARSE_H
#define GRAMMARS_GRAMMAR_PARSE_H

#include <grammars/grammar.h>

#include <stddef.h>
#include <stdio.h>

typedef enum {
	GRAMMAR_PARSE_OK,
	GRAMMAR_PARSE_ERR_INVALID,
	GRAMMAR_PARSE_ERR_MEMORY,
	GRAMMAR_PARSE_ERR_IO,
} GrammarParseResult;

typedef struct {
	size_t      line;
	const char *message;
} GrammarParseError;

GrammarParseResult grammar_read(Grammar *grammar, FILE *stream, GrammarParseError *error);

#endif

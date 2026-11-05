#ifndef GRAMMARS_GRAMMAR_H
#define GRAMMARS_GRAMMAR_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t TerminalId;
typedef uint32_t NonterminalId;

typedef enum {
	SYMBOL_TERMINAL,
	SYMBOL_NONTERMINAL,
} SymbolKind;

typedef struct {
	SymbolKind kind;
	uint32_t   id;
} Symbol;

typedef struct {
	NonterminalId lhs;
	size_t        rhs_offset;
	size_t        rhs_count;
} GrammarRule;

#define VEC(type, name) type *name; size_t name##_count; size_t name##_cap

typedef struct {
	VEC(char,        nonterminals);
	VEC(char,        terminals);
	VEC(GrammarRule, rules);
	VEC(Symbol,      rhs);

	NonterminalId start;
	int           has_start;
} /* context-free */ Grammar;

typedef enum {
	GRAMMAR_OK,
	GRAMMAR_ERR_INVALID,
	GRAMMAR_ERR_MEMORY,
} GrammarResult;

GrammarResult grammar_add_nonterminal(Grammar *grammar, char name);
GrammarResult grammar_add_terminal   (Grammar *grammar, char name);
GrammarResult grammar_set_start      (Grammar *grammar, char name);

GrammarResult grammar_add_rule(Grammar *grammar, char lhs, const char *rhs);

void grammar_free(Grammar *grammar);

#endif

#include <grammars/grammar.h>

#include "vec.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const size_t DEFAULT_CAP = 8;

static int is_nonterminal(char name) {
	return name >= 'A' && name <= 'Z';
}

static int is_terminal(char name) {
	unsigned char c = (unsigned char)name;
	return c >= 0x21 && c <= 0x7e && !is_nonterminal(name);
}

static size_t find_name(const char *names, size_t count, char name) {
	for (size_t i = 0; i < count; ++i) {
		if (names[i] == name) {
			return i;
		}
	}

	return count;
}

GrammarResult grammar_add_nonterminal(Grammar *grammar, char name) {
	assert(grammar != NULL);

	if (!is_nonterminal(name)) {
		return GRAMMAR_ERR_INVALID;
	}

	if (find_name(grammar->nonterminals,
	              grammar->nonterminals_count,
	              name) != grammar->nonterminals_count) {
		return GRAMMAR_ERR_INVALID;
	}

	if (grammar->nonterminals_count >= UINT32_MAX) {
		return GRAMMAR_ERR_MEMORY;
	}

	if (grammar->nonterminals_count == grammar->nonterminals_cap) {
		size_t new_cap;
		if (!vec_next_cap(grammar->nonterminals_cap, DEFAULT_CAP, &new_cap)) {
			return GRAMMAR_ERR_MEMORY;
		}

		char *names = vec_realloc(grammar->nonterminals, new_cap, sizeof *names);
		if (names == NULL) {
			return GRAMMAR_ERR_MEMORY;
		}

		grammar->nonterminals     = names;
		grammar->nonterminals_cap = new_cap;
	}

	grammar->nonterminals[grammar->nonterminals_count++] = name;
	return GRAMMAR_OK;
}

GrammarResult grammar_add_terminal(Grammar *grammar, char name) {
	assert(grammar != NULL);

	if (!is_terminal(name)) {
		return GRAMMAR_ERR_INVALID;
	}

	if (find_name(grammar->terminals,
	              grammar->terminals_count,
	              name) != grammar->terminals_count) {
		return GRAMMAR_ERR_INVALID;
	}

	if (grammar->terminals_count >= UINT32_MAX) {
		return GRAMMAR_ERR_MEMORY;
	}

	if (grammar->terminals_count == grammar->terminals_cap) {
		size_t new_cap;
		if (!vec_next_cap(grammar->terminals_cap, DEFAULT_CAP, &new_cap)) {
			return GRAMMAR_ERR_MEMORY;
		}

		char *names = vec_realloc(grammar->terminals, new_cap, sizeof *names);
		if (names == NULL) {
			return GRAMMAR_ERR_MEMORY;
		}

		grammar->terminals     = names;
		grammar->terminals_cap = new_cap;
	}

	grammar->terminals[grammar->terminals_count++] = name;
	return GRAMMAR_OK;
}

GrammarResult grammar_set_start(Grammar *grammar, char name) {
	assert(grammar != NULL);

	size_t id = find_name(grammar->nonterminals, grammar->nonterminals_count, name);
	if (id == grammar->nonterminals_count) {
		return GRAMMAR_ERR_INVALID;
	}

	grammar->start = (NonterminalId)id;
	grammar->has_start = 1;
	return GRAMMAR_OK;
}

static GrammarResult reserve_rule(Grammar *grammar) {
	if (grammar->rules_count < grammar->rules_cap) {
		return GRAMMAR_OK;
	}

	size_t new_cap;
	if (!vec_next_cap(grammar->rules_cap, DEFAULT_CAP, &new_cap)) {
		return GRAMMAR_ERR_MEMORY;
	}

	GrammarRule *rules = vec_realloc(grammar->rules, new_cap, sizeof *rules);
	if (rules == NULL) {
		return GRAMMAR_ERR_MEMORY;
	}

	grammar->rules     = rules;
	grammar->rules_cap = new_cap;
	return GRAMMAR_OK;
}

static GrammarResult reserve_rhs(Grammar *grammar, size_t count) {
	if (count > SIZE_MAX - grammar->rhs_count) {
		return GRAMMAR_ERR_MEMORY;
	}

	size_t needed = grammar->rhs_count + count;
	if (needed <= grammar->rhs_cap) {
		return GRAMMAR_OK;
	}

	size_t new_cap = grammar->rhs_cap;
	while (new_cap < needed) {
		if (!vec_next_cap(new_cap, DEFAULT_CAP, &new_cap)) {
			return GRAMMAR_ERR_MEMORY;
		}
	}

	Symbol *rhs = vec_realloc(grammar->rhs, new_cap, sizeof *rhs);
	if (rhs == NULL) {
		return GRAMMAR_ERR_MEMORY;
	}

	grammar->rhs     = rhs;
	grammar->rhs_cap = new_cap;
	return GRAMMAR_OK;
}

GrammarResult grammar_add_rule(Grammar *grammar, char lhs, const char *rhs) {
	assert(grammar != NULL);
	assert(rhs     != NULL);

	size_t lhs_id = find_name(grammar->nonterminals, grammar->nonterminals_count, lhs);
	if (lhs_id == grammar->nonterminals_count) {
		return GRAMMAR_ERR_INVALID;
	}

	size_t length = strlen(rhs);

	for (size_t i = 0; i < length; ++i) {
		char name = rhs[i];
		if (is_nonterminal(name)) {
			if (find_name(grammar->nonterminals,
			              grammar->nonterminals_count,
			              name) == grammar->nonterminals_count) {
				return GRAMMAR_ERR_INVALID;
			}
		} else if (find_name(grammar->terminals,
		                     grammar->terminals_count,
		                     name) == grammar->terminals_count) {
			return GRAMMAR_ERR_INVALID;
		}
	}

	if (reserve_rule(grammar) != GRAMMAR_OK || reserve_rhs(grammar, length) != GRAMMAR_OK) {
		return GRAMMAR_ERR_MEMORY;
	}

	size_t offset = grammar->rhs_count;

	for (size_t i = 0; i < length; ++i) {
		char name = rhs[i];
		Symbol symbol;

		if (is_nonterminal(name)) {
			symbol = (Symbol){
				.kind = SYMBOL_NONTERMINAL,
				.id = (uint32_t)find_name(grammar->nonterminals,
				                          grammar->nonterminals_count, name),
			};
		} else {
			symbol = (Symbol){
				.kind = SYMBOL_TERMINAL,
				.id = (uint32_t)find_name(grammar->terminals,
				                          grammar->terminals_count, name),
			};
		}

		grammar->rhs[offset + i] = symbol;
	}

	grammar->rules[grammar->rules_count++] = (GrammarRule){
		.lhs        = (NonterminalId)lhs_id,
		.rhs_offset = offset,
		.rhs_count  = length,
	};
	grammar->rhs_count += length;

	return GRAMMAR_OK;
}

void grammar_free(Grammar *grammar) {
	assert(grammar != NULL);

	free(grammar->nonterminals);
	free(grammar->terminals);
	free(grammar->rules);
	free(grammar->rhs);

	*grammar = (Grammar){0};
}

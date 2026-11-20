#include <grammars/earley.h>

#include "vec.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
	ALPHABET_SIZE = 256,
	INITIAL_SLOT_CAP = 16,
};

static size_t item_hash(EarleyItem item) {
	size_t hash = item.rule * (size_t)2654435761u;
	hash ^= item.dot    + (hash << 6) + (hash >> 2);
	hash ^= item.origin + (hash << 6) + (hash >> 2);
	return hash;
}

static int item_equal(EarleyItem left, EarleyItem right) {
	return left.rule   == right.rule &&
	       left.dot    == right.dot  &&
	       left.origin == right.origin;
}

static int set_rehash(EarleySet *set, size_t cap) {
	assert(cap != 0 && (cap & (cap - 1)) == 0);

	if (cap > SIZE_MAX / sizeof *set->slots) {
		return 0;
	}

	size_t *slots = calloc(cap, sizeof *slots);
	if (slots == NULL) {
		return 0;
	}

	for (size_t i = 0; i < set->items_count; ++i) {
		size_t pos = item_hash(set->items[i]) & (cap - 1);
		while (slots[pos] != 0) {
			pos = (pos + 1) & (cap - 1);
		}
		slots[pos] = i + 1;
	}

	free(set->slots);
	set->slots    = slots;
	set->slot_cap = cap;
	return 1;
}

static int set_add(EarleySet *set, EarleyItem item) {
	if (set->slot_cap == 0 && !set_rehash(set, INITIAL_SLOT_CAP)) {
		return 0;
	}

	size_t pos = item_hash(item) & (set->slot_cap - 1);
	while (set->slots[pos] != 0) {
		EarleyItem existing = set->items[set->slots[pos] - 1];
		if (item_equal(existing, item)) {
			return 1;
		}
		pos = (pos + 1) & (set->slot_cap - 1);
	}

	if (set->items_count >= set->slot_cap / 2) {
		if (set->slot_cap > SIZE_MAX / 2 || !set_rehash(set, set->slot_cap * 2)) {
			return 0;
		}
		pos = item_hash(item) & (set->slot_cap - 1);
		while (set->slots[pos] != 0) {
			pos = (pos + 1) & (set->slot_cap - 1);
		}
	}

	if (set->items_count == set->items_cap) {
		size_t cap;
		if (!vec_next_cap(set->items_cap, 8, &cap)) {
			return 0;
		}

		EarleyItem *items = vec_realloc(set->items, cap, sizeof *items);
		if (items == NULL) {
			return 0;
		}
		set->items     = items;
		set->items_cap = cap;
	}

	set->items[set->items_count] = item;
	set->slots[pos] = set->items_count + 1;
	set->items_count++;
	return 1;
}

void earley_chart_free(EarleyChart *chart) {
	assert(chart != NULL);

	for (size_t i = 0; i < chart->set_count; ++i) {
		free(chart->sets[i].items);
		free(chart->sets[i].slots);
	}
	free(chart->sets);
	*chart = (EarleyChart){0};
}

EarleyResult earley_fit(Earley *earley, const Grammar *grammar) {
	assert(earley  != NULL);
	assert(grammar != NULL);

	if (earley->grammar.nonterminals_count != 0) {
		return EARLEY_ERR_INVALID;
	}

	Grammar       copy   = {0};
	GrammarResult result = grammar_clone(&copy, grammar);

	if (result == GRAMMAR_ERR_INVALID) {
		return EARLEY_ERR_INVALID;
	}
	if (result != GRAMMAR_OK) {
		return EARLEY_ERR_MEMORY;
	}

	unsigned char *nullable = calloc(copy.nonterminals_count, sizeof *nullable);
	if (nullable == NULL) {
		grammar_free(&copy);
		return EARLEY_ERR_MEMORY;
	}

	int changed;
	do {
		changed = 0;
		for (size_t i = 0; i < copy.rules_count; ++i) {
			const GrammarRule *rule = &copy.rules[i];
			if (nullable[rule->lhs]) {
				continue;
			}

			int derives_empty = 1;
			for (size_t j = 0; j < rule->rhs_count; ++j) {
				const Symbol *symbol = &copy.rhs[rule->rhs_offset + j];
				if (symbol->kind == SYMBOL_TERMINAL || !nullable[symbol->id]) {
					derives_empty = 0;
					break;
				}
			}
			if (derives_empty) {
				nullable[rule->lhs] = 1;
				changed = 1;
			}
		}
	} while (changed);

	earley->grammar  = copy;
	earley->nullable = nullable;
	return EARLEY_OK;
}

static int predict(EarleySet *set, const Grammar *grammar,
                   NonterminalId nonterminal, size_t position) {
	for (size_t i = 0; i < grammar->rules_count; ++i) {
		if (grammar->rules[i].lhs == nonterminal &&
		    !set_add(set, (EarleyItem){.rule = i, .dot = 0, .origin = position})) {
			return 0;
		}
	}
	return 1;
}

static int complete(EarleyChart *chart, const Grammar *grammar,
                    EarleyItem completed, size_t position)
{
	const GrammarRule *rule        = &grammar->rules[completed.rule];
	NonterminalId      nonterminal = rule->lhs;
	EarleySet         *previous    = &chart->sets[completed.origin];
	EarleySet         *current     = &chart->sets[position];

	for (size_t i = 0; i < previous->items_count; ++i) {
		EarleyItem waiting = previous->items[i];
		const GrammarRule *other = &grammar->rules[waiting.rule];
		if (waiting.dot == other->rhs_count) {
			continue;
		}
		Symbol symbol = grammar->rhs[other->rhs_offset + waiting.dot];
		if (symbol.kind == SYMBOL_NONTERMINAL && symbol.id == nonterminal) {
			if (!set_add(current, (EarleyItem){
				.rule   = waiting.rule,
				.dot    = waiting.dot + 1,
				.origin = waiting.origin,
			})) {
				return 0;
			}
		}
	}
	return 1;
}

EarleyResult earley_build_chart(
	EarleyChart  *chart,
	const Earley *earley,
	const char   *word,
	size_t        length
) {
	assert(chart  != NULL);
	assert(earley != NULL);
	assert(word   != NULL || length == 0);

	assert(chart->sets      == NULL);
	assert(chart->set_count == 0);

	const Grammar *grammar = &earley->grammar;
	if (!grammar->has_start || grammar->nonterminals_count == 0) {
		return EARLEY_ERR_INVALID;
	}

	TerminalId index[ALPHABET_SIZE];
	for (size_t i = 0; i < ALPHABET_SIZE; ++i) {
		index[i] = UINT32_MAX;
	}
	for (size_t i = 0; i < grammar->terminals_count; ++i) {
		index[(unsigned char)grammar->terminals[i]] = (TerminalId)i;
	}
	for (size_t i = 0; i < length; ++i) {
		if (index[(unsigned char)word[i]] == UINT32_MAX) {
			return EARLEY_ERR_INVALID;
		}
	}

	if (length == SIZE_MAX || length + 1 > SIZE_MAX / sizeof(EarleySet)) {
		return EARLEY_ERR_MEMORY;
	}

	EarleyChart result = {0};
	result.set_count = length + 1;
	result.sets      = calloc(result.set_count, sizeof *result.sets);
	if (result.sets == NULL) {
		return EARLEY_ERR_MEMORY;
	}

	for (size_t i = 0; i < grammar->rules_count; ++i) {
		if (grammar->rules[i].lhs == grammar->start &&
		    !set_add(&result.sets[0], (EarleyItem){.rule = i, .dot = 0, .origin = 0})) {
			goto fail;
		}
	}

	for (size_t pos = 0; pos <= length; ++pos) {
		EarleySet *set = &result.sets[pos];
		for (size_t i = 0; i < set->items_count; ++i) {
			EarleyItem item = set->items[i];
			const GrammarRule *rule = &grammar->rules[item.rule];

			if (item.dot == rule->rhs_count) {
				if (!complete(&result, grammar, item, pos)) {
					goto fail;
				}
				continue;
			}

			Symbol symbol = grammar->rhs[rule->rhs_offset + item.dot];
			if (symbol.kind == SYMBOL_NONTERMINAL) {
				if (!predict(set, grammar, symbol.id, pos)) {
					goto fail;
				}

				if (earley->nullable[symbol.id] && !set_add(set, (EarleyItem){
					.rule   = item.rule,
					.dot    = item.dot + 1,
					.origin = item.origin,
				})) {
					goto fail;
				}
			} else if (pos < length && symbol.id == index[(unsigned char)word[pos]]) {
				if (!set_add(&result.sets[pos + 1], (EarleyItem){
					.rule   = item.rule,
					.dot    = item.dot + 1,
					.origin = item.origin,
				})) {
					goto fail;
				}
			}
		}
	}

	*chart = result;
	return EARLEY_OK;

fail:
	earley_chart_free(&result);
	return EARLEY_ERR_MEMORY;
}

int earley_chart_accepts(const EarleyChart *chart, const Earley *earley) {
	assert(chart  != NULL);
	assert(earley != NULL);
	assert(chart->set_count > 0);

	const Grammar   *grammar = &earley->grammar;
	const EarleySet *last    = &chart->sets[chart->set_count - 1];
	for (size_t i = 0; i < last->items_count; ++i) {
		EarleyItem item = last->items[i];
		const GrammarRule *rule = &grammar->rules[item.rule];
		if (rule->lhs == grammar->start && item.origin == 0 && item.dot == rule->rhs_count){
			return 1;
		}
	}
	return 0;
}

EarleyResult earley_predict(const Earley *earley, const char *word, size_t length, int *accepts) {
	assert(accepts != NULL);

	EarleyChart chart = {0};
	EarleyResult result = earley_build_chart(&chart, earley, word, length);
	if (result != EARLEY_OK) {
		return result;
	}

	*accepts = earley_chart_accepts(&chart, earley);
	earley_chart_free(&chart);
	return EARLEY_OK;
}

void earley_free(Earley *earley) {
	assert(earley != NULL);

	grammar_free(&earley->grammar);
	free(earley->nullable);
	*earley = (Earley){0};
}

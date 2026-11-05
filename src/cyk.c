#include <grammars/cyk.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static uint64_t *cyk_cell(CykTable *table, size_t start, size_t span) {
	assert(table != NULL);
	assert(span > 0);
	assert(start < table->length);
	assert(span <= table->length - start);

	return &table->cells[(start * table->length + span - 1) * table->words_per_cell];
}

static int cell_contains(const uint64_t *cell, NonterminalId nonterminal) {
	size_t word = (size_t)nonterminal / 64;
	size_t bit  = (size_t)nonterminal % 64;

	return (cell[word] & (1UL << bit)) != 0;
}

static void cell_add(uint64_t *cell, NonterminalId nonterminal) {
	size_t word = (size_t)nonterminal / 64;
	size_t bit  = (size_t)nonterminal % 64;

	cell[word] |= 1UL << bit;
}

CykResult cyk_build_table(
	CykTable         *table,
	const CnfGrammar *grammar,
	const char       *word,
	size_t            length
) {
	assert(table   != NULL);
	assert(grammar != NULL);
	assert(word    != NULL || length == 0);

	assert(table->cells  == NULL);
	assert(table->length == 0);

	if (grammar->nonterminal_count == 0 || grammar->start >= grammar->nonterminal_count) {
		return CYK_ERR_INVALID;
	}

	if (length == 0) {
		return CYK_OK;
	}

	TerminalId terminal_index[256];

	for (size_t i = 0; i < 256; ++i) {
		terminal_index[i] = UINT32_MAX;
	}

	for (size_t i = 0; i < grammar->terminal_count; ++i) {
		unsigned char symbol   = (unsigned char)grammar->terminal_labels[i];
		terminal_index[symbol] = (TerminalId)i;
	}

	for (size_t i = 0; i < length; ++i) {
		if (terminal_index[(unsigned char)word[i]] == UINT32_MAX) {
			return CYK_ERR_INVALID;
		}
	}

	size_t words_per_cell = grammar->nonterminal_count / 64 +
	               (size_t)(grammar->nonterminal_count % 64 != 0);

	if (length > SIZE_MAX / length) {
		return CYK_ERR_MEMORY;
	}

	size_t cell_count = length * length;

	if (cell_count > SIZE_MAX / words_per_cell) {
		return CYK_ERR_MEMORY;
	}

	size_t word_count = cell_count * words_per_cell;

	if (word_count > SIZE_MAX / sizeof(uint64_t)) {
		return CYK_ERR_MEMORY;
	}

	uint64_t *cells = calloc(word_count, sizeof *cells);
	if (cells == NULL) {
		return CYK_ERR_MEMORY;
	}

	CykTable result = {
		.length            = length,
		.nonterminal_count = grammar->nonterminal_count,
		.words_per_cell    = words_per_cell,
		.cells             = cells,
	};

	/* base */
	for (size_t start = 0; start < length; ++start) {
		TerminalId terminal = terminal_index[(unsigned char)word[start]];
		uint64_t  *cell     = cyk_cell(&result, start, 1);

		for (size_t i = 0; i < grammar->terminal_rule_count; ++i) {
			const CnfTerminalRule *rule = &grammar->terminal_rules[i];

			if (rule->terminal == terminal) {
				cell_add(cell, rule->lhs);
			}
		}
	}

	/* induction */
	for (size_t span = 2; span <= length; ++span) {
		for (size_t start = 0; start <= length - span; ++start) {
			uint64_t *cell = cyk_cell(&result, start, span);

			for (size_t split = 1; split < span; ++split) {
				const uint64_t *left  = cyk_cell(&result, start, split);
				const uint64_t *right = cyk_cell(&result, start +split, span -split);

				for (size_t i = 0; i < grammar->binary_rule_count; ++i) {
					const CnfBinaryRule *rule = &grammar->binary_rules[i];

					if (cell_contains(left,  rule->left) &&
					    cell_contains(right, rule->right)) {
						cell_add(cell, rule->lhs);
					}
				}
			}
		}
	}

	*table = result;
	return CYK_OK;
}

int cyk_table_contains(
	const CykTable *table,
	size_t          start,
	size_t          span,
	NonterminalId   nonterminal
) {
	assert(table        != NULL);
	assert(table->cells != NULL);

	assert((size_t)nonterminal < table->nonterminal_count);
	assert(span > 0);
	assert(start < table->length);
	assert(span <= table->length - start);

	const uint64_t *cell = &table->cells[
		(start * table->length + span - 1) * table->words_per_cell
	];

	return cell_contains(cell, nonterminal);
}

int cyk_table_accepts(const CykTable *table, const CnfGrammar *grammar) {
	assert(table   != NULL);
	assert(grammar != NULL);

	assert(table->length == 0 || table->nonterminal_count == grammar->nonterminal_count);
	assert(grammar->start < grammar->nonterminal_count);

	if (table->length == 0) {
		return grammar->start_nullable != 0;
	}

	return cyk_table_contains(table, 0, table->length, grammar->start);
}

void cyk_table_free(CykTable *table) {
	assert(table != NULL);

	free(table->cells);
	*table = (CykTable){0};
}

CykResult cyk_fit(Cyk *cyk, const Grammar *grammar) {
	assert(cyk     != NULL);
	assert(grammar != NULL);

	if (cyk->grammar.nonterminal_count != 0) {
		return CYK_ERR_INVALID;
	}

	switch (grammar_to_cnf(&cyk->grammar, grammar)) {
	case CNF_OK:          return CYK_OK;
	case CNF_ERR_INVALID: return CYK_ERR_INVALID;
	case CNF_ERR_MEMORY:  return CYK_ERR_MEMORY;
	}

	return CYK_ERR_INVALID;
}

CykResult cyk_predict(const Cyk *cyk, const char *word, size_t length, int *accepts) {
	assert(cyk     != NULL);
	assert(word    != NULL || length == 0);
	assert(accepts != NULL);

	if (cyk->grammar.nonterminal_count == 0) {
		return CYK_ERR_INVALID;
	}

	if (length == 0) {
		*accepts = cyk->grammar.start_nullable != 0;
		return CYK_OK;
	}

	CykTable  table  = {0};
	CykResult result = cyk_build_table(&table, &cyk->grammar, word, length);

	if (result != CYK_OK) {
		return result;
	}

	*accepts = cyk_table_accepts(&table, &cyk->grammar);

	cyk_table_free(&table);
	return CYK_OK;
}

void cyk_free(Cyk *cyk) {
	assert(cyk != NULL);

	cnf_free(&cyk->grammar);
}

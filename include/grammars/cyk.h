#ifndef GRAMMARS_CYK_H
#define GRAMMARS_CYK_H

#include <grammars/cnf.h>

#include <stddef.h>
#include <stdint.h>

typedef enum {
	CYK_OK,
	CYK_ERR_INVALID,
	CYK_ERR_MEMORY,
} CykResult;

typedef struct {
	CnfGrammar grammar;
} Cyk;

typedef struct {
	size_t    length;
	size_t    nonterminal_count;
	size_t    words_per_cell;
	uint64_t *cells;
} CykTable;

CykResult cyk_fit(Cyk *cyk, const Grammar *grammar);

CykResult cyk_predict(
	const Cyk  *cyk,
	const char *word,
	size_t      length,
	int        *accepts
);

void cyk_free(Cyk *cyk);

CykResult cyk_build_table(
	CykTable         *table,
	const CnfGrammar *grammar,
	const char       *word,
	size_t            length
);

int cyk_table_contains(
	const CykTable *table,
	size_t          start,
	size_t          span,
	NonterminalId   nonterminal
);

int cyk_table_accepts(const CykTable *table, const CnfGrammar *grammar);

void cyk_table_free(CykTable *table);

#endif

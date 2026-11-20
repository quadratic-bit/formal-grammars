#ifndef GRAMMARS_EARLEY_H
#define GRAMMARS_EARLEY_H

#include <grammars/grammar.h>

#include <stddef.h>

typedef enum {
	EARLEY_OK,
	EARLEY_ERR_INVALID,
	EARLEY_ERR_MEMORY,
} EarleyResult;

typedef struct {
	size_t rule;
	size_t dot;
	size_t origin;
} EarleyItem;

typedef struct {
	VEC(EarleyItem, items);

	size_t *slots;
	size_t  slot_cap;
} EarleySet;

typedef struct {
	EarleySet *sets;
	size_t     set_count;
} EarleyChart;

typedef struct {
	Grammar        grammar;
	unsigned char *nullable;
} Earley;

EarleyResult earley_fit(Earley *earley, const Grammar *grammar);

EarleyResult earley_predict(const Earley *earley, const char *word, size_t length, int *accepts);

EarleyResult earley_build_chart(
	EarleyChart  *chart,
	const Earley *earley,
	const char   *word,
	size_t        length
);

int earley_chart_accepts(const EarleyChart *chart, const Earley *earley);

void earley_chart_free(EarleyChart *chart);
void earley_free      (Earley      *earley);

#endif

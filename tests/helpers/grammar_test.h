#ifndef GRAMMAR_TEST_HELPERS_H
#define GRAMMAR_TEST_HELPERS_H

#include <grammars/cnf.h>
#include <grammars/cyk.h>
#include <grammars/grammar.h>

Grammar test_grammar(
	const char *nonterminals,
	const char *terminals,
	char start
);

void test_add_rule(Grammar *grammar, char lhs, const char *rhs);

CnfGrammar test_to_cnf(const Grammar *grammar);

void test_assert_cnf (const CnfGrammar *cnf);
int  test_cnf_accepts(const CnfGrammar *cnf, const char *word);

Cyk test_fit(const Grammar *grammar);
int test_predict(const Cyk *cyk, const char *word);

NonterminalId test_find_nonterminal(const CnfGrammar *cnf, char label);

#endif

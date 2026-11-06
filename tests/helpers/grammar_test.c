#include "grammar_test.h"

#include <criterion/criterion.h>

#include <stddef.h>
#include <string.h>

Grammar test_grammar(const char *nonterminals, const char *terminals, char start) {
	Grammar grammar = {0};

	for (size_t i = 0; nonterminals[i] != '\0'; ++i) {
		cr_assert_eq(grammar_add_nonterminal(&grammar, nonterminals[i]), GRAMMAR_OK);
	}

	for (size_t i = 0; terminals[i] != '\0'; ++i) {
		cr_assert_eq(grammar_add_terminal(&grammar, terminals[i]), GRAMMAR_OK);
	}

	cr_assert_eq(grammar_set_start(&grammar, start), GRAMMAR_OK);
	return grammar;
}

void test_add_rule(Grammar *grammar, char lhs, const char *rhs) {
	cr_assert_eq(
		grammar_add_rule(grammar, lhs, rhs),
		GRAMMAR_OK,
		"failed to add rule %c -> %s",
		lhs, rhs
	);
}

CnfGrammar test_to_cnf(const Grammar *grammar) {
	CnfGrammar cnf = {0};

	cr_assert_eq(grammar_to_cnf(&cnf, grammar), CNF_OK);
	return cnf;
}

void test_assert_cnf(const CnfGrammar *cnf) {
	cr_assert_gt(cnf->nonterminal_count, 0);
	cr_assert_lt(cnf->start, cnf->nonterminal_count);
	cr_assert_not_null(cnf->nonterminal_labels);

	if (cnf->terminal_count != 0) {
		cr_assert_not_null(cnf->terminal_labels);
	}

	for (size_t i = 0; i < cnf->binary_rule_count; ++i) {
		const CnfBinaryRule *rule = &cnf->binary_rules[i];

		cr_assert_lt(rule->lhs,   cnf->nonterminal_count);
		cr_assert_lt(rule->left,  cnf->nonterminal_count);
		cr_assert_lt(rule->right, cnf->nonterminal_count);

		cr_assert_neq(rule->left,  cnf->start);
		cr_assert_neq(rule->right, cnf->start);

		for (size_t j = 0; j < i; ++j) {
			const CnfBinaryRule *other = &cnf->binary_rules[j];

			cr_assert(
				rule->lhs   != other->lhs  ||
				rule->left  != other->left ||
				rule->right != other->right,
				"duplicate CNF binary rule"
			);
		}
	}

	for (size_t i = 0; i < cnf->terminal_rule_count; ++i) {
		const CnfTerminalRule *rule = &cnf->terminal_rules[i];

		cr_assert_lt(rule->lhs,      cnf->nonterminal_count);
		cr_assert_lt(rule->terminal, cnf->terminal_count);

		for (size_t j = 0; j < i; ++j) {
			const CnfTerminalRule *other = &cnf->terminal_rules[j];

			cr_assert(
				rule->lhs      != other->lhs ||
				rule->terminal != other->terminal,
				"duplicate CNF terminal rule"
			);
		}
	}
}

NonterminalId test_find_nonterminal(const CnfGrammar *cnf, char label) {
	for (size_t i = 0; i < cnf->nonterminal_count; ++i) {
		if (cnf->nonterminal_labels[i] == label) {
			return (NonterminalId)i;
		}
	}

	cr_assert(0, "nonterminal '%c' not found", label);
	return 0;
}

int test_cnf_accepts(const CnfGrammar *cnf, const char *word) {
	CykTable table = {0};

	cr_assert_eq(
		cyk_build_table(&table, cnf, word, strlen(word)),
		CYK_OK,
		"failed to check word \"%s\"",
		word
	);

	int accepts = cyk_table_accepts(&table, cnf);
	cyk_table_free(&table);
	return accepts;
}

Cyk test_fit(const Grammar *grammar) {
	Cyk cyk = {0};

	cr_assert_eq(cyk_fit(&cyk, grammar), CYK_OK);
	return cyk;
}

int test_predict(const Cyk *cyk, const char *word) {
	int accepts = -1;

	cr_assert_eq(
		cyk_predict(cyk, word, strlen(word), &accepts),
		CYK_OK,
		"failed to predict \"%s\"",
		word
	);

	cr_assert(accepts == 0 || accepts == 1);
	return accepts;
}

#include <grammars/grammar.h>

#include "helpers/grammar_test.h"

#include <criterion/criterion.h>

#include <stddef.h>

Test(grammar, rules_and_epsilon) {
	Grammar grammar = test_grammar("SAB", "ab", 'S');

	test_add_rule(&grammar, 'S', "aSbS");
	test_add_rule(&grammar, 'S', "");

	cr_assert_eq(grammar.nonterminals_count, 3);
	cr_assert_eq(grammar.terminals_count,    2);
	cr_assert_eq(grammar.rules_count,        2);
	cr_assert_eq(grammar.rhs_count,          4);

	cr_assert_eq(grammar.rules[0].lhs, grammar.start);
	cr_assert_eq(grammar.rules[0].rhs_offset, 0);
	cr_assert_eq(grammar.rules[0].rhs_count,  4);

	cr_assert_eq(grammar.rhs[0].kind, SYMBOL_TERMINAL);
	cr_assert_eq(grammar.rhs[0].id, 0);

	cr_assert_eq(grammar.rhs[1].kind, SYMBOL_NONTERMINAL);
	cr_assert_eq(grammar.rhs[1].id, grammar.start);

	cr_assert_eq(grammar.rhs[2].kind, SYMBOL_TERMINAL);
	cr_assert_eq(grammar.rhs[2].id, 1);

	cr_assert_eq(grammar.rhs[3].kind, SYMBOL_NONTERMINAL);
	cr_assert_eq(grammar.rhs[3].id, grammar.start);

	cr_assert_eq(grammar.rules[1].rhs_offset, 4);
	cr_assert_eq(grammar.rules[1].rhs_count,  0);

	grammar_free(&grammar);
}

Test(grammar, rejects_invalid_declarations) {
	Grammar grammar = test_grammar("S", "a", 'S');

	cr_assert_eq(grammar_add_nonterminal(&grammar, 'S'), GRAMMAR_ERR_INVALID);
	cr_assert_eq(grammar_add_nonterminal(&grammar, 'a'), GRAMMAR_ERR_INVALID);

	cr_assert_eq(grammar_add_terminal(&grammar, 'a'), GRAMMAR_ERR_INVALID);
	cr_assert_eq(grammar_add_terminal(&grammar, 'A'), GRAMMAR_ERR_INVALID);
	cr_assert_eq(grammar_add_terminal(&grammar, ' '), GRAMMAR_ERR_INVALID);

	cr_assert_eq(grammar.nonterminals_count, 1);
	cr_assert_eq(grammar.terminals_count,    1);

	grammar_free(&grammar);
}

Test(grammar, rejects_undeclared_symbols) {
	Grammar grammar = test_grammar("S", "a", 'S');

	test_add_rule(&grammar, 'S', "a");

	cr_assert_eq(grammar_add_rule(&grammar, 'A', "a"), GRAMMAR_ERR_INVALID);
	cr_assert_eq(grammar_add_rule(&grammar, 'S', "b"), GRAMMAR_ERR_INVALID);
	cr_assert_eq(grammar_add_rule(&grammar, 'S', "A"), GRAMMAR_ERR_INVALID);

	cr_assert_eq(grammar_set_start(&grammar, 'A'), GRAMMAR_ERR_INVALID);

	cr_assert_eq(grammar.rules_count, 1);
	cr_assert_eq(grammar.rhs_count,   1);
	cr_assert_eq(grammar.start,       0);

	grammar_free(&grammar);
}

Test(grammar, grows_rule_and_rhs_storage) {
	Grammar grammar = test_grammar("S", "a", 'S');

	for (size_t i = 0; i < 40; ++i) {
		test_add_rule(&grammar, 'S', "a");
	}

	cr_assert_eq(grammar.rules_count, 40);
	cr_assert_eq(grammar.rhs_count,   40);

	for (size_t i = 0; i < 40; ++i) {
		cr_assert_eq(grammar.rules[i].rhs_offset, i);
		cr_assert_eq(grammar.rules[i].rhs_count, 1);
		cr_assert_eq(grammar.rhs[i].kind, SYMBOL_TERMINAL);
		cr_assert_eq(grammar.rhs[i].id, 0);
	}

	grammar_free(&grammar);

	cr_assert_null(grammar.rules);
	cr_assert_null(grammar.rhs);
	cr_assert_eq(grammar.rules_count, 0);
}

Test(grammar, zero_rules_is_valid) {
	Grammar grammar = test_grammar("S", "", 'S');

	cr_assert   (grammar.has_start);
	cr_assert_eq(grammar.rules_count, 0);

	grammar_free(&grammar);
}

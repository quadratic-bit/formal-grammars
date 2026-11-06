#include <grammars/cnf.h>

#include "helpers/grammar_test.h"

#include <criterion/criterion.h>

Test(cnf, simple_binary_grammar) {
	Grammar grammar = test_grammar("SAB", "ab", 'S');

	test_add_rule(&grammar, 'S', "AB");
	test_add_rule(&grammar, 'A', "a");
	test_add_rule(&grammar, 'B', "b");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(!cnf.start_nullable);

	cr_assert_eq(cnf.binary_rule_count,   1);
	cr_assert_eq(cnf.terminal_rule_count, 2);

	NonterminalId a = test_find_nonterminal(&cnf, 'A');
	NonterminalId b = test_find_nonterminal(&cnf, 'B');

	cr_assert_eq(cnf.binary_rules[0].lhs, cnf.start);
	cr_assert_eq(cnf.binary_rules[0].left, a);
	cr_assert_eq(cnf.binary_rules[0].right, b);

	cr_assert( test_cnf_accepts(&cnf, "ab"));
	cr_assert(!test_cnf_accepts(&cnf, ""));
	cr_assert(!test_cnf_accepts(&cnf, "ba"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, eliminates_nullable_symbols) {
	Grammar grammar = test_grammar("SAB", "ab", 'S');

	test_add_rule(&grammar, 'S', "AB");
	test_add_rule(&grammar, 'A', "");
	test_add_rule(&grammar, 'A', "a");
	test_add_rule(&grammar, 'B', "");
	test_add_rule(&grammar, 'B', "b");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(cnf.start_nullable);

	cr_assert(test_cnf_accepts(&cnf, ""));
	cr_assert(test_cnf_accepts(&cnf, "a"));
	cr_assert(test_cnf_accepts(&cnf, "b"));
	cr_assert(test_cnf_accepts(&cnf, "ab"));

	cr_assert(!test_cnf_accepts(&cnf, "ba"));
	cr_assert(!test_cnf_accepts(&cnf, "aa"));
	cr_assert(!test_cnf_accepts(&cnf, "bb"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, eliminates_unit_cycles) {
	Grammar grammar = test_grammar("SAB", "b", 'S');

	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'A', "B");
	test_add_rule(&grammar, 'B', "A");
	test_add_rule(&grammar, 'B', "");
	test_add_rule(&grammar, 'B', "b");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(cnf.start_nullable);
	cr_assert( test_cnf_accepts(&cnf, ""));
	cr_assert( test_cnf_accepts(&cnf, "b"));
	cr_assert(!test_cnf_accepts(&cnf, "bb"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, isolates_terminals_and_binarizes) {
	Grammar grammar = test_grammar("SBD", "abcd", 'S');

	test_add_rule(&grammar, 'S', "aBcD");
	test_add_rule(&grammar, 'B', "b");
	test_add_rule(&grammar, 'D', "d");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert_gt(cnf.binary_rule_count, 0);
	cr_assert(test_cnf_accepts(&cnf, "abcd"));

	cr_assert(!test_cnf_accepts(&cnf, ""));
	cr_assert(!test_cnf_accepts(&cnf, "abc"));
	cr_assert(!test_cnf_accepts(&cnf, "acd"));
	cr_assert(!test_cnf_accepts(&cnf, "abdc"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, nullable_symbol_in_long_rule) {
	Grammar grammar = test_grammar("SABCD", "acd", 'S');

	test_add_rule(&grammar, 'S', "ABCD");
	test_add_rule(&grammar, 'A', "a");
	test_add_rule(&grammar, 'B', "");
	test_add_rule(&grammar, 'C', "c");
	test_add_rule(&grammar, 'D', "d");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(test_cnf_accepts(&cnf, "acd"));

	cr_assert(!test_cnf_accepts(&cnf, ""));
	cr_assert(!test_cnf_accepts(&cnf, "ac"));
	cr_assert(!test_cnf_accepts(&cnf, "ad"));
	cr_assert(!test_cnf_accepts(&cnf, "cd"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, removes_useless_symbols) {
	Grammar grammar = test_grammar("SABC", "ac", 'S');

	test_add_rule(&grammar, 'S', "a");
	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'A', "B");
	test_add_rule(&grammar, 'B', "B");
	test_add_rule(&grammar, 'C', "c");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert_eq(cnf.nonterminal_count, 1);
	cr_assert_eq(cnf.binary_rule_count, 0);
	cr_assert_eq(cnf.terminal_rule_count, 1);

	cr_assert_eq(cnf.terminal_count, 2);

	cr_assert(test_cnf_accepts(&cnf, "a"));
	cr_assert(!test_cnf_accepts(&cnf, "c"));
	cr_assert(!test_cnf_accepts(&cnf, ""));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, removes_duplicate_rules) {
	Grammar grammar = test_grammar("SAB", "a", 'S');

	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'S', "B");
	test_add_rule(&grammar, 'A', "a");
	test_add_rule(&grammar, 'B', "a");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert_eq(cnf.terminal_rule_count, 1);
	cr_assert(test_cnf_accepts(&cnf, "a"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, original_start_can_occur_on_rhs) {
	Grammar grammar = test_grammar("S", "a", 'S');

	test_add_rule(&grammar, 'S', "aS");
	test_add_rule(&grammar, 'S', "a");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(test_cnf_accepts(&cnf, "a"));
	cr_assert(test_cnf_accepts(&cnf, "aa"));
	cr_assert(test_cnf_accepts(&cnf, "aaaa"));

	cr_assert(!test_cnf_accepts(&cnf, ""));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, empty_language) {
	Grammar grammar = test_grammar("S", "a", 'S');
	CnfGrammar cnf = test_to_cnf(&grammar);

	test_assert_cnf(&cnf);

	cr_assert(!cnf.start_nullable);
	cr_assert_eq(cnf.binary_rule_count, 0);
	cr_assert_eq(cnf.terminal_rule_count, 0);

	cr_assert(!test_cnf_accepts(&cnf, ""));
	cr_assert(!test_cnf_accepts(&cnf, "a"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, epsilon_only_language) {
	Grammar grammar = test_grammar("S", "a", 'S');

	test_add_rule(&grammar, 'S', "");

	CnfGrammar cnf = test_to_cnf(&grammar);
	test_assert_cnf(&cnf);

	cr_assert(cnf.start_nullable);
	cr_assert_eq(cnf.binary_rule_count, 0);
	cr_assert_eq(cnf.terminal_rule_count, 0);

	cr_assert( test_cnf_accepts(&cnf, ""));
	cr_assert(!test_cnf_accepts(&cnf, "a"));

	cnf_free(&cnf);
	grammar_free(&grammar);
}

Test(cnf, rejects_missing_start) {
	Grammar    grammar = {0};
	CnfGrammar cnf     = {0};

	cr_assert_eq(grammar_add_nonterminal(&grammar, 'S'), GRAMMAR_OK);

	cr_assert_eq(grammar_to_cnf(&cnf, &grammar), CNF_ERR_INVALID);

	cr_assert_null(cnf.nonterminal_labels);
	cr_assert_null(cnf.binary_rules);

	cnf_free(&cnf);
	grammar_free(&grammar);
}

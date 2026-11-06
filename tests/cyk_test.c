#include <grammars/cyk.h>

#include "helpers/grammar_test.h"

#include <criterion/criterion.h>

Test(cyk, exact_table_for_ab) {
	char nonterminals[] = {'S', 'A', 'B'};
	char terminals   [] = {'a', 'b'};

	CnfBinaryRule binary[] = {
		{.lhs = 0, .left = 1, .right = 2},
	};

	CnfTerminalRule terminal_rules[] = {
		{.lhs = 1, .terminal = 0},
		{.lhs = 2, .terminal = 1},
	};

	CnfGrammar cnf = {
		.start = 0,
		.nonterminal_labels = nonterminals,
		.nonterminal_count = 3,
		.terminal_labels = terminals,
		.terminal_count = 2,
		.binary_rules = binary,
		.binary_rule_count = 1,
		.terminal_rules = terminal_rules,
		.terminal_rule_count = 2,
	};

	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "ab", 2), CYK_OK);

	/* T[0,1] = {A} */
	cr_assert( cyk_table_contains(&table, 0, 1, 1));
	cr_assert(!cyk_table_contains(&table, 0, 1, 0));
	cr_assert(!cyk_table_contains(&table, 0, 1, 2));

	/* T[1,1] = {B} */
	cr_assert( cyk_table_contains(&table, 1, 1, 2));
	cr_assert(!cyk_table_contains(&table, 1, 1, 0));
	cr_assert(!cyk_table_contains(&table, 1, 1, 1));

	/* T[0,2] = {S} */
	cr_assert( cyk_table_contains(&table, 0, 2, 0));
	cr_assert(!cyk_table_contains(&table, 0, 2, 1));
	cr_assert(!cyk_table_contains(&table, 0, 2, 2));

	cr_assert(cyk_table_accepts(&table, &cnf));

	cyk_table_free(&table);
}

Test(cyk, ambiguous_grammar) {
	char nonterminals[] = {'S'};
	char terminals   [] = {'a'};

	CnfBinaryRule binary[] = {
		{.lhs = 0, .left = 0, .right = 0},
	};

	CnfTerminalRule terminal_rules[] = {
		{.lhs = 0, .terminal = 0},
	};

	CnfGrammar cnf = {
		.start = 0,
		.nonterminal_labels = nonterminals,
		.nonterminal_count = 1,
		.terminal_labels = terminals,
		.terminal_count = 1,
		.binary_rules = binary,
		.binary_rule_count = 1,
		.terminal_rules = terminal_rules,
		.terminal_rule_count = 1,
	};

	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "aaa", 3), CYK_OK);

	for (size_t start = 0; start < 3; ++start) {
		cr_assert(cyk_table_contains(&table, start, 1, 0));
	}

	cr_assert(cyk_table_contains(&table, 0, 2, 0));
	cr_assert(cyk_table_contains(&table, 1, 2, 0));
	cr_assert(cyk_table_contains(&table, 0, 3, 0));

	cr_assert(cyk_table_accepts(&table, &cnf));

	cyk_table_free(&table);
}

Test(cyk, several_nonterminals_in_one_cell) {
	char nonterminals[] = {'S', 'A', 'B', 'C', 'D'};
	char terminals   [] = {'a', 'b'};

	CnfBinaryRule binary[] = {
		{.lhs = 0, .left = 1, .right = 2},
		{.lhs = 0, .left = 3, .right = 4},
	};

	CnfTerminalRule terminal_rules[] = {
		{.lhs = 1, .terminal = 0},
		{.lhs = 3, .terminal = 0},
		{.lhs = 2, .terminal = 1},
		{.lhs = 4, .terminal = 1},
	};

	CnfGrammar cnf = {
		.start = 0,
		.nonterminal_labels = nonterminals,
		.nonterminal_count = 5,
		.terminal_labels = terminals,
		.terminal_count = 2,
		.binary_rules = binary,
		.binary_rule_count = 2,
		.terminal_rules = terminal_rules,
		.terminal_rule_count = 4,
	};

	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "ab", 2), CYK_OK);

	cr_assert(cyk_table_contains(&table, 0, 1, 1));
	cr_assert(cyk_table_contains(&table, 0, 1, 3));

	cr_assert(cyk_table_contains(&table, 1, 1, 2));
	cr_assert(cyk_table_contains(&table, 1, 1, 4));

	cr_assert(cyk_table_contains(&table, 0, 2, 0));
	cr_assert(cyk_table_accepts(&table, &cnf));

	cyk_table_free(&table);
}

Test(cyk, bitset_spans_multiple_words) {
	char nonterminals[70] = {0};
	char terminals   [  ] = {'a'};

	CnfTerminalRule terminal_rules[] = {
		{.lhs = 69, .terminal = 0},
	};

	CnfGrammar cnf = {
		.start = 69,
		.nonterminal_labels = nonterminals,
		.nonterminal_count = 70,
		.terminal_labels = terminals,
		.terminal_count = 1,
		.terminal_rules = terminal_rules,
		.terminal_rule_count = 1,
	};

	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "a", 1), CYK_OK);

	cr_assert_eq(table.words_per_cell, 2);
	cr_assert( cyk_table_contains(&table, 0, 1, 69));
	cr_assert(!cyk_table_contains(&table, 0, 1, 0));
	cr_assert( cyk_table_accepts (&table, &cnf));

	cyk_table_free(&table);
}

Test(cyk, empty_word_uses_start_nullable) {
	char nonterminals[] = {'S'};

	CnfGrammar cnf = {
		.start = 0,
		.start_nullable = 1,
		.nonterminal_labels = nonterminals,
		.nonterminal_count = 1,
	};

	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "", 0), CYK_OK);

	cr_assert_null(table.cells);
	cr_assert_eq(table.length, 0);
	cr_assert(cyk_table_accepts(&table, &cnf));

	cnf.start_nullable = 0;
	cr_assert(!cyk_table_accepts(&table, &cnf));

	cyk_table_free(&table);
}

Test(cyk, predicts_several_words_after_one_fit) {
	Grammar grammar = test_grammar("S", "ab", 'S');

	test_add_rule(&grammar, 'S', "aSbS");
	test_add_rule(&grammar, 'S', "");

	Cyk cyk = test_fit(&grammar);
	grammar_free(&grammar);

	cr_assert(test_predict(&cyk, ""));
	cr_assert(test_predict(&cyk, "ab"));
	cr_assert(test_predict(&cyk, "aabb"));
	cr_assert(test_predict(&cyk, "abab"));
	cr_assert(test_predict(&cyk, "aababb"));

	cr_assert(!test_predict(&cyk, "a"));
	cr_assert(!test_predict(&cyk, "abb"));
	cr_assert(!test_predict(&cyk, "ba"));
	cr_assert(!test_predict(&cyk, "aabbba"));

	cyk_free(&cyk);
}

Test(cyk, balanced_parentheses) {
	Grammar grammar = test_grammar("S", "()", 'S');

	test_add_rule(&grammar, 'S', "(S)S");
	test_add_rule(&grammar, 'S', "");

	Cyk cyk = test_fit(&grammar);
	grammar_free(&grammar);

	cr_assert(test_predict(&cyk, ""));
	cr_assert(test_predict(&cyk, "()"));
	cr_assert(test_predict(&cyk, "(())"));
	cr_assert(test_predict(&cyk, "()()"));
	cr_assert(test_predict(&cyk, "(()())"));

	cr_assert(!test_predict(&cyk, "("));
	cr_assert(!test_predict(&cyk, "())"));
	cr_assert(!test_predict(&cyk, "(()"));
	cr_assert(!test_predict(&cyk, ")("));

	cyk_free(&cyk);
}

Test(cyk, valid_word_can_be_rejected) {
	Grammar grammar = test_grammar("S", "ab", 'S');
	test_add_rule(&grammar, 'S', "a");

	Cyk cyk = test_fit(&grammar);
	grammar_free(&grammar);

	cr_assert( test_predict(&cyk, "a"));
	cr_assert(!test_predict(&cyk, "b"));
	cr_assert(!test_predict(&cyk, ""));
	cr_assert(!test_predict(&cyk, "aa"));

	cyk_free(&cyk);
}

Test(cyk, undeclared_terminal_is_an_error) {
	Grammar grammar = test_grammar("S", "ab", 'S');
	test_add_rule(&grammar, 'S', "a");

	Cyk cyk = test_fit(&grammar);
	grammar_free(&grammar);

	int accepts = 42;

	cr_assert_eq(cyk_predict(&cyk, "c", 1, &accepts), CYK_ERR_INVALID);
	cr_assert_eq(accepts, 42);

	cyk_free(&cyk);
}

Test(cyk, rejects_invalid_prepared_grammar) {
	CnfGrammar cnf = {0};
	CykTable table = {0};

	cr_assert_eq(cyk_build_table(&table, &cnf, "a", 1), CYK_ERR_INVALID);

	cr_assert_null(table.cells);
	cyk_table_free(&table);
}

Test(cyk, cannot_fit_twice_without_freeing) {
	Grammar grammar = test_grammar("S", "a", 'S');
	test_add_rule(&grammar, 'S', "a");

	Cyk cyk = test_fit(&grammar);

	cr_assert_eq(cyk_fit(&cyk, &grammar), CYK_ERR_INVALID);

	cr_assert(test_predict(&cyk, "a"));

	cyk_free(&cyk);
	grammar_free(&grammar);
}

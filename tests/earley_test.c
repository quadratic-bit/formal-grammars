#include <grammars/earley.h>

#include "helpers/grammar_test.h"

#include <criterion/criterion.h>

#include <stddef.h>
#include <string.h>

static Earley fit_earley(const Grammar *grammar) {
	Earley earley = {0};
	cr_assert_eq(earley_fit(&earley, grammar), EARLEY_OK);
	return earley;
}

static int accepts(const Earley *earley, const char *word) {
	int result = -1;
	cr_assert_eq(earley_predict(earley, word, strlen(word), &result), EARLEY_OK);
	return result;
}

static int contains_item(const EarleySet *set, size_t rule, size_t dot, size_t origin) {
	for (size_t i = 0; i < set->items_count; ++i) {
		EarleyItem item = set->items[i];
		if (item.rule == rule && item.dot == dot && item.origin == origin) {
			return 1;
		}
	}
	return 0;
}

Test(earley, predictor_scanner_completer) {
	Grammar grammar = test_grammar("SAB", "ab", 'S');
	test_add_rule(&grammar, 'S', "AB");
	test_add_rule(&grammar, 'A', "a");
	test_add_rule(&grammar, 'B', "b");

	Earley earley = fit_earley(&grammar);
	EarleyChart chart = {0};
	cr_assert_eq(earley_build_chart(&chart, &earley, "ab", 2), EARLEY_OK);
	cr_assert_eq(chart.set_count, 3);

	cr_assert(contains_item(&chart.sets[0], 0, 0, 0));
	cr_assert(contains_item(&chart.sets[0], 1, 0, 0));
	cr_assert(contains_item(&chart.sets[1], 1, 1, 0));
	cr_assert(contains_item(&chart.sets[1], 0, 1, 0));
	cr_assert(contains_item(&chart.sets[1], 2, 0, 1));
	cr_assert(contains_item(&chart.sets[2], 2, 1, 1));
	cr_assert(contains_item(&chart.sets[2], 0, 2, 0));
	cr_assert(earley_chart_accepts(&chart, &earley));

	earley_chart_free(&chart);
	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, nullable_completed_before_waiting) {
	Grammar grammar = test_grammar("SAB", "a", 'S');
	test_add_rule(&grammar, 'S', "AB");
	test_add_rule(&grammar, 'A', "");
	test_add_rule(&grammar, 'B', "A");

	Earley earley = fit_earley(&grammar);
	EarleyChart chart = {0};
	cr_assert_eq(earley_build_chart(&chart, &earley, "", 0), EARLEY_OK);

	cr_assert(contains_item(&chart.sets[0], 2, 1, 0));
	cr_assert(contains_item(&chart.sets[0], 0, 2, 0));

	cr_assert(earley_chart_accepts(&chart, &earley));

	cr_assert(!accepts(&earley, "a"));

	earley_chart_free(&chart);
	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, consecutive_nullable_nonterminals) {
	Grammar grammar = test_grammar("SABC", "a", 'S');
	test_add_rule(&grammar, 'S', "ABC");
	test_add_rule(&grammar, 'A', "");
	test_add_rule(&grammar, 'B', "");
	test_add_rule(&grammar, 'C', "");

	Earley earley = fit_earley(&grammar);
	cr_assert( accepts(&earley, ""));
	cr_assert(!accepts(&earley, "a"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, direct_left_recursion) {
	Grammar grammar = test_grammar("S", "a", 'S');
	test_add_rule(&grammar, 'S', "Sa");
	test_add_rule(&grammar, 'S', "a");

	Earley earley = fit_earley(&grammar);
	cr_assert(!accepts(&earley, ""));
	cr_assert( accepts(&earley, "a"));
	cr_assert( accepts(&earley, "aa"));
	cr_assert( accepts(&earley, "aaaa"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, indirect_left_recursion) {
	Grammar grammar = test_grammar("SA", "a", 'S');
	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'A', "Sa");
	test_add_rule(&grammar, 'A', "a");

	Earley earley = fit_earley(&grammar);
	cr_assert(!accepts(&earley, ""));
	cr_assert( accepts(&earley, "a"));
	cr_assert( accepts(&earley, "aa"));
	cr_assert( accepts(&earley, "aaa"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, unit_cycle) {
	Grammar grammar = test_grammar("SAB", "a", 'S');
	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'A', "B");
	test_add_rule(&grammar, 'B', "A");
	test_add_rule(&grammar, 'B', "a");

	Earley earley = fit_earley(&grammar);
	cr_assert( accepts(&earley, "a"));
	cr_assert(!accepts(&earley, ""));
	cr_assert(!accepts(&earley, "aa"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, nullable_cycle) {
	Grammar grammar = test_grammar("SAB", "a", 'S');
	test_add_rule(&grammar, 'S', "A");
	test_add_rule(&grammar, 'A', "B");
	test_add_rule(&grammar, 'B', "A");
	test_add_rule(&grammar, 'B', "");

	Earley earley = fit_earley(&grammar);
	cr_assert( accepts(&earley, ""));
	cr_assert(!accepts(&earley, "a"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, ambiguous_grammar) {
	Grammar grammar = test_grammar("S", "a", 'S');
	test_add_rule(&grammar, 'S', "SS");
	test_add_rule(&grammar, 'S', "a");

	Earley earley = fit_earley(&grammar);
	cr_assert(!accepts(&earley, ""));
	cr_assert( accepts(&earley, "a"));
	cr_assert( accepts(&earley, "aa"));
	cr_assert( accepts(&earley, "aaa"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, cyclic_empty_language_deduplicates) {
	Grammar grammar = test_grammar("S", "a", 'S');
	test_add_rule(&grammar, 'S', "S");
	test_add_rule(&grammar, 'S', "S");

	Earley earley = fit_earley(&grammar);
	EarleyChart chart = {0};
	cr_assert_eq(earley_build_chart(&chart, &earley, "", 0), EARLEY_OK);
	cr_assert_eq(chart.sets[0].items_count, 2);
	cr_assert(!earley_chart_accepts(&chart, &earley));
	cr_assert(!accepts(&earley, "a"));

	earley_chart_free(&chart);
	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, empty_language_with_no_rules) {
	Grammar grammar = test_grammar("S", "a", 'S');
	Earley earley   = fit_earley(&grammar);

	cr_assert(!accepts(&earley, ""));
	cr_assert(!accepts(&earley, "a"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, fit_owns_grammar_and_reuses_it) {
	Grammar grammar = test_grammar("S", "ab", 'S');
	test_add_rule(&grammar, 'S', "aSbS");
	test_add_rule(&grammar, 'S', "");

	Earley earley = fit_earley(&grammar);
	grammar_free(&grammar);

	cr_assert( accepts(&earley, ""));
	cr_assert( accepts(&earley, "ab"));
	cr_assert( accepts(&earley, "aabb"));
	cr_assert( accepts(&earley, "abab"));
	cr_assert(!accepts(&earley, "aabbba"));
	cr_assert(!accepts(&earley, "ba"));

	earley_free(&earley);
}

Test(earley, invalid_terminal_is_not_rejection) {
	Grammar grammar = test_grammar("S", "a", 'S');
	test_add_rule(&grammar, 'S', "a");

	Earley earley = fit_earley(&grammar);
	int result = 23;

	cr_assert_eq(earley_predict(&earley, "b", 1, &result), EARLEY_ERR_INVALID);
	cr_assert_eq(result, 23);
	cr_assert(!accepts(&earley, ""));
	cr_assert(!accepts(&earley, "aa"));

	earley_free(&earley);
	grammar_free(&grammar);
}

Test(earley, refuses_unfitted_or_repeated_fit) {
	Earley earley = {0};
	int result = 23;

	cr_assert_eq(earley_predict(&earley, "", 0, &result), EARLEY_ERR_INVALID);

	Grammar invalid = {0};
	cr_assert_eq(earley_fit(&earley, &invalid), EARLEY_ERR_INVALID);

	Grammar grammar = test_grammar("S", "a", 'S');
	cr_assert_eq(earley_fit(&earley, &grammar), EARLEY_OK);
	cr_assert_eq(earley_fit(&earley, &grammar), EARLEY_ERR_INVALID);

	earley_free(&earley);
	grammar_free(&grammar);
}

static void compare_with_cyk(Grammar *grammar) {
	Earley earley = fit_earley(grammar);
	Cyk    cyk    = test_fit  (grammar);

	char word[7] = {0};

	for (size_t length = 0; length <= 6; ++length) {
		for (size_t bits = 0; bits < ((size_t)1 << length); ++bits) {
			for (size_t i = 0; i < length; ++i) {
				word[i] = (bits & ((size_t)1 << i)) ? 'b' : 'a';
			}
			word[length] = '\0';

			cr_assert_eq(
				accepts(&earley, word),
				test_predict(&cyk, word),
				"Earley and CYK disagree on '%s'",
				word
			);
		}
	}

	cyk_free(&cyk);
	earley_free(&earley);
}

Test(earley, agrees_with_cyk_balanced_ab) {
	Grammar grammar = test_grammar("S", "ab", 'S');
	test_add_rule(&grammar, 'S', "aSbS");
	test_add_rule(&grammar, 'S', "");

	compare_with_cyk(&grammar);
	grammar_free(&grammar);
}

Test(earley, agrees_with_cyk_left_recursive) {
	Grammar grammar = test_grammar("S", "ab", 'S');
	test_add_rule(&grammar, 'S', "Sa");
	test_add_rule(&grammar, 'S', "b");

	compare_with_cyk(&grammar);
	grammar_free(&grammar);
}

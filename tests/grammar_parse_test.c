#include <grammars/grammar_parse.h>

#include <criterion/criterion.h>

#include <stdio.h>
#include <string.h>

static FILE *open_input(const char *source) {
	FILE *file = tmpfile();
	cr_assert_not_null(file);

	size_t length = strlen(source);

	cr_assert_eq(fwrite(source, 1, length, file), length);

	rewind(file);
	return file;
}

static GrammarParseResult parse_input(
	const char *source,
	Grammar *grammar,
	GrammarParseError *error
) {
	FILE *file = open_input(source);

	GrammarParseResult result = grammar_read(grammar, file, error);

	fclose(file);
	return result;
}

Test(grammar_parse, valid_grammar) {
	static const char *source =
		"3 2 3\n"
		"S A B\n"
		"a b\n"
		"S -> A B\n"
		"A -> a\n"
		"B -> b\n"
		"S\n";

	Grammar           grammar = {0};
	GrammarParseError error   = {0};

	cr_assert_eq(parse_input(source, &grammar, &error), GRAMMAR_PARSE_OK);

	cr_assert_eq(grammar.nonterminals_count, 3);
	cr_assert_eq(grammar.terminals_count,    2);
	cr_assert_eq(grammar.rules_count,        3);
	cr_assert_eq(grammar.rhs_count,          4);
	cr_assert_eq(grammar.start,              0);

	grammar_free(&grammar);
}

Test(grammar_parse, epsilon_and_whitespace) {
	static const char *source =
		"1 2 2\n"
		"S\n"
		"ab\n"
		" S   ->  a S b S \n"
		" S   ->   \n"
		"S\n";

	Grammar           grammar = {0};
	GrammarParseError error   = {0};

	cr_assert_eq(parse_input(source, &grammar, &error), GRAMMAR_PARSE_OK);

	cr_assert_eq(grammar.rules_count,        2);
	cr_assert_eq(grammar.rules[0].rhs_count, 4);
	cr_assert_eq(grammar.rules[1].rhs_count, 0);

	grammar_free(&grammar);
}

Test(grammar_parse, zero_terminals) {
	static const char *source =
		"1 0 1\n"
		"S\n"
		"\n"
		"S ->\n"
		"S\n";

	Grammar           grammar = {0};
	GrammarParseError error   = {0};

	cr_assert_eq(parse_input(source, &grammar, &error), GRAMMAR_PARSE_OK);

	cr_assert_eq(grammar.terminals_count,    0);
	cr_assert_eq(grammar.rules_count,        1);
	cr_assert_eq(grammar.rules[0].rhs_count, 0);

	grammar_free(&grammar);
}

Test(grammar_parse, leaves_words_in_stream) {
	static const char *source =
		"1 2 2\n"
		"S\n"
		"ab\n"
		"S -> aSbS\n"
		"S ->\n"
		"S\n"
		"2\n"
		"\n"
		"ab\n";

	FILE *file = open_input(source);

	Grammar           grammar = {0};
	GrammarParseError error   = {0};

	cr_assert_eq(grammar_read(&grammar, file, &error), GRAMMAR_PARSE_OK);

	char line[32];

	cr_assert_not_null(fgets(line, sizeof line, file));
	cr_assert_str_eq(line, "2\n");

	cr_assert_not_null(fgets(line, sizeof line, file));
	cr_assert_str_eq(line, "\n");

	cr_assert_not_null(fgets(line, sizeof line, file));
	cr_assert_str_eq(line, "ab\n");

	grammar_free(&grammar);
	fclose(file);
}

Test(grammar_parse, rejects_invalid_grammars) {
	static const char *cases[] = {
		"0 1 0\nS\na\nS\n",
		"-1 1 0\nS\na\nS\n",
		"101 1 0\nS\na\nS\n",
		"1 1\nS\na\nS\n",
		"1 1 0\nSS\na\nS\n",
		"1 2 0\nS\naa\nS\n",
		"1 1 1\nS\na\nS -> b\nS\n",
		"1 1 1\nS\na\nA -> a\nS\n",
		"1 1 1\nS\na\nS - a\nS\n",
		"1 1 0\nS\na\nA\n",
		"1 1 0\nS\n@\nS\n",
		"1 1 1\nS\na\nS -> a\n",
	};

	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
		Grammar grammar = {0};
		GrammarParseError error = {0};

		cr_assert_eq(
			parse_input(cases[i], &grammar, &error),
			GRAMMAR_PARSE_ERR_INVALID,
			"accepted invalid grammar:\n%s",
			cases[i]
		);

		cr_assert_gt      (error.line, 0);
		cr_assert_not_null(error.message);

		cr_assert_eq  (grammar.rules_count, 0);
		cr_assert_null(grammar.rules);

		grammar_free(&grammar);
	}
}

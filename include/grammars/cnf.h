#ifndef GRAMMARS_CNF_H
#define GRAMMARS_CNF_H

#include <grammars/grammar.h>

#include <stddef.h>

/* A -> BC */
typedef struct {
	NonterminalId lhs;

	NonterminalId left;
	NonterminalId right;
} CnfBinaryRule;

/* A -> a */
typedef struct {
	NonterminalId lhs;
	TerminalId    terminal;
} CnfTerminalRule;

typedef struct {
	NonterminalId start;
	int           start_nullable;

	char  *nonterminal_labels;
	size_t nonterminal_count;

	char  *terminal_labels;
	size_t terminal_count;

	CnfBinaryRule *binary_rules;
	size_t         binary_rule_count;

	CnfTerminalRule *terminal_rules;
	size_t           terminal_rule_count;
} CnfGrammar;

typedef enum {
	CNF_OK,
	CNF_ERR_INVALID,
	CNF_ERR_MEMORY,
} CnfResult;

CnfResult grammar_to_cnf(CnfGrammar *cnf, const Grammar *grammar);

void cnf_free(CnfGrammar *cnf);

#endif

#include <grammars/cnf.h>

#include "vec.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const size_t DEFAULT_CAP = 8;

typedef struct {
	GrammarRule *rules;
	size_t       rule_count;
	size_t       rule_cap;

	Symbol *rhs;
	size_t  rhs_count;
	size_t  rhs_cap;

	size_t        nonterminal_count;
	NonterminalId start;
} WorkGrammar;

static void work_free(WorkGrammar *grammar) {
	free(grammar->rules);
	free(grammar->rhs);
	*grammar = (WorkGrammar){0};
}

static void work_replace(WorkGrammar *grammar, WorkGrammar *replacement) {
	work_free(grammar);

	*grammar     = *replacement;
	*replacement = (WorkGrammar){0};
}

static int work_grow_rules(WorkGrammar *grammar) {
	size_t cap;
	if (!vec_next_cap(grammar->rule_cap, DEFAULT_CAP, &cap)) {
		return 0;
	}

	GrammarRule *rules = vec_realloc(grammar->rules, cap, sizeof *rules);
	if (rules == NULL) {
		return 0;
	}

	grammar->rules    = rules;
	grammar->rule_cap = cap;
	return 1;
}

static int work_reserve_rhs(WorkGrammar *grammar, size_t count) {
	if (count > SIZE_MAX - grammar->rhs_count) {
		return 0;
	}

	size_t needed = grammar->rhs_count + count;
	size_t cap    = grammar->rhs_cap;
	while (cap < needed) {
		if (!vec_next_cap(cap, DEFAULT_CAP, &cap)) {
			return 0;
		}
	}

	if (cap == grammar->rhs_cap) {
		return 1;
	}

	Symbol *rhs = vec_realloc(grammar->rhs, cap, sizeof *rhs);
	if (rhs == NULL) {
		return 0;
	}

	grammar->rhs     = rhs;
	grammar->rhs_cap = cap;
	return 1;
}

static int work_add_rule(WorkGrammar *grammar, NonterminalId lhs, const Symbol *rhs, size_t count) {
	assert(rhs != NULL || count == 0);
	assert(lhs < grammar->nonterminal_count);

	if (grammar->rule_count == SIZE_MAX)                                       return 0;
	if (grammar->rule_count == grammar->rule_cap && !work_grow_rules(grammar)) return 0;
	if (!work_reserve_rhs(grammar, count))                                     return 0;

	if (count != 0) {
		memcpy(&grammar->rhs[grammar->rhs_count], rhs, count * sizeof *rhs);
	}

	grammar->rules[grammar->rule_count++] = (GrammarRule){
		.lhs        = lhs,
		.rhs_offset = grammar->rhs_count,
		.rhs_count  = count,
	};
	grammar->rhs_count += count;
	return 1;
}

static const Symbol *work_rhs(const WorkGrammar *grammar, const GrammarRule *rule) {
	return rule->rhs_count != 0 ? &grammar->rhs[rule->rhs_offset] : NULL;
}

static int work_add_unique(
	WorkGrammar  *grammar,
	NonterminalId lhs,
	const Symbol *rhs,
	size_t        count
) {
	for (size_t i = 0; i < grammar->rule_count; ++i) {
		const GrammarRule *rule = &grammar->rules[i];
		if (rule->lhs != lhs || rule->rhs_count != count) {
			continue;
		}

		const Symbol *existing = work_rhs(grammar, rule);
		size_t j = 0;
		for (; j < count; ++j) {
			if (existing[j].kind != rhs[j].kind || existing[j].id != rhs[j].id) {
				break;
			}
		}
		if (j == count) {
			return 1;
		}
	}

	return work_add_rule(grammar, lhs, rhs, count);
}

static int work_new_nonterminal(WorkGrammar *grammar, NonterminalId *id) {
	if (grammar->nonterminal_count >= UINT32_MAX) {
		return 0;
	}

	*id = (NonterminalId)grammar->nonterminal_count++;
	return 1;
}

static int work_from_grammar(WorkGrammar *work, const Grammar *source) {
	work->nonterminal_count = source->nonterminals_count;

	NonterminalId start;
	if (!work_new_nonterminal(work, &start)) {
		return 0;
	}

	work->start = start;
	Symbol old_start = {.kind = SYMBOL_NONTERMINAL, .id = source->start};
	if (!work_add_rule(work, start, &old_start, 1)) {
		return 0;
	}

	for (size_t i = 0; i < source->rules_count; ++i) {
		const GrammarRule *rule = &source->rules[i];

		const Symbol *rhs = rule->rhs_count != 0
		                  ? &source->rhs[rule->rhs_offset]
		                  : NULL;

		if (!work_add_rule(work, rule->lhs, rhs, rule->rhs_count)) {
			return 0;
		}
	}
	return 1;
}

static int work_isolate_terminals(WorkGrammar *grammar, size_t terminal_count) {
	WorkGrammar out = {
		.nonterminal_count = grammar->nonterminal_count,
		.start             = grammar->start,
	};

	NonterminalId *proxy = NULL;
	if (terminal_count != 0) {
		if (terminal_count > SIZE_MAX / sizeof *proxy) {
			return 0;
		}

		proxy = malloc(terminal_count * sizeof *proxy);
		if (proxy == NULL) {
			return 0;
		}

		for (size_t i = 0; i < terminal_count; ++i) {
			proxy[i] = UINT32_MAX;
		}
	}

	for (size_t i = 0; i < grammar->rule_count; ++i) {
		const GrammarRule *rule = &grammar->rules[i];
		const Symbol      *rhs  = work_rhs(grammar, rule);

		if (!work_add_rule(&out, rule->lhs, rhs, rule->rhs_count)) {
			goto fail;
		}

		if (rule->rhs_count < 2) {
			continue;
		}

		size_t offset = out.rules[out.rule_count - 1].rhs_offset;
		for (size_t j = 0; j < rule->rhs_count; ++j) {
			Symbol symbol = rhs[j];
			if (symbol.kind != SYMBOL_TERMINAL) {
				continue;
			}

			assert(symbol.id < terminal_count);
			if (proxy[symbol.id] == UINT32_MAX) {
				NonterminalId id;
				if (!work_new_nonterminal(&out, &id) ||
				    !work_add_rule       (&out,  id, &symbol, 1)) {
					goto fail;
				}
				proxy[symbol.id] = id;
			}

			out.rhs[offset + j] = (Symbol){
				.kind = SYMBOL_NONTERMINAL,
				.id   = proxy[symbol.id],
			};
		}
	}

	free(proxy);
	work_replace(grammar, &out);
	return 1;

fail:
	free(proxy);
	work_free(&out);
	return 0;
}

static int work_chain(WorkGrammar *grammar) {
	WorkGrammar out = {
		.nonterminal_count = grammar->nonterminal_count,
		.start             = grammar->start,
	};

	for (size_t i = 0; i < grammar->rule_count; ++i) {
		const GrammarRule *rule = &grammar->rules[i];
		const Symbol      *rhs  = work_rhs(grammar, rule);

		if (rule->rhs_count <= 2) {
			if (!work_add_rule(&out, rule->lhs, rhs, rule->rhs_count)) {
				goto fail;
			}
			continue;
		}

		NonterminalId lhs = rule->lhs;
		for (size_t j = 0; j + 2 < rule->rhs_count; ++j) {
			NonterminalId next;
			if (!work_new_nonterminal(&out, &next)) {
				goto fail;
			}
			Symbol pair[2] = {
				rhs[j],
				{
					.kind = SYMBOL_NONTERMINAL,
					.id   = next
				},
			};
			if (!work_add_rule(&out, lhs, pair, 2)) {
				goto fail;
			}
			lhs = next;
		}

		if (!work_add_rule(&out, lhs, &rhs[rule->rhs_count - 2], 2)) {
			goto fail;
		}
	}

	work_replace(grammar, &out);
	return 1;

fail:
	work_free(&out);
	return 0;
}

static int work_remove_epsilon(WorkGrammar *grammar, int *start_nullable) {
	unsigned char *nullable = calloc(grammar->nonterminal_count, sizeof *nullable);
	if (nullable == NULL) {
		return 0;
	}

	int changed;
	do {
		changed = 0;
		for (size_t i = 0; i < grammar->rule_count; ++i) {
			const GrammarRule *rule = &grammar->rules[i];
			if (nullable[rule->lhs]) {
				continue;
			}

			const Symbol *rhs = work_rhs(grammar, rule);
			int empty = 1;
			for (size_t j = 0; j < rule->rhs_count; ++j) {
				if (rhs[j].kind == SYMBOL_TERMINAL || !nullable[rhs[j].id]) {
					empty = 0;
					break;
				}
			}
			if (empty) {
				nullable[rule->lhs] = 1;
				changed             = 1;
			}
		}
	} while (changed);

	WorkGrammar out = {
		.nonterminal_count = grammar->nonterminal_count,
		.start             = grammar->start,
	};

	for (size_t i = 0; i < grammar->rule_count; ++i) {
		const GrammarRule *rule = &grammar->rules[i];
		const Symbol      *rhs  = work_rhs(grammar, rule);

		if (rule->rhs_count == 0) {
			continue;
		}

		if (!work_add_rule(&out, rule->lhs, rhs, rule->rhs_count)) {
			goto fail;
		}

		if (rule->rhs_count == 2) {
			if (rhs[0].kind == SYMBOL_NONTERMINAL &&
			    nullable[rhs[0].id]               &&
			    !work_add_rule(&out, rule->lhs, &rhs[1], 1)) {
				goto fail;
			}
			if (rhs[1].kind == SYMBOL_NONTERMINAL &&
			    nullable[rhs[1].id]               &&
			    !work_add_rule(&out, rule->lhs, &rhs[0], 1)) {
				goto fail;
			}
		}
	}

	*start_nullable = nullable[grammar->start] != 0;
	free(nullable);
	work_replace(grammar, &out);
	return 1;

fail:
	free(nullable);
	work_free(&out);
	return 0;
}

static int is_unit_rule(const WorkGrammar *grammar, const GrammarRule *rule) {
	if (rule->rhs_count != 1) {
		return 0;
	}
	return grammar->rhs[rule->rhs_offset].kind == SYMBOL_NONTERMINAL;
}

static int work_remove_units(WorkGrammar *grammar) {
	size_t count = grammar->nonterminal_count;
	if (count > SIZE_MAX / sizeof(NonterminalId)) {
		return 0;
	}

	unsigned char *visited = calloc(count,  sizeof *visited);
	NonterminalId *stack   = malloc(count * sizeof *stack);
	if (visited == NULL || stack == NULL) {
		free(visited);
		free(stack);
		return 0;
	}

	WorkGrammar out = {
		.nonterminal_count = count,
		.start             = grammar->start,
	};

	for (NonterminalId lhs = 0; lhs < count; ++lhs) {
		memset(visited, 0, count * sizeof *visited);
		size_t stack_len   = 0;
		visited[lhs]       = 1;
		stack[stack_len++] = lhs;

		while (stack_len != 0) {
			NonterminalId current = stack[--stack_len];

			for (size_t i = 0; i < grammar->rule_count; ++i) {
				const GrammarRule *rule = &grammar->rules[i];
				if (rule->lhs != current) {
					continue;
				}

				const Symbol *rhs = work_rhs(grammar, rule);
				if (is_unit_rule(grammar, rule)) {
					NonterminalId target = rhs[0].id;
					if (!visited[target]) {
						visited[target]    = 1;
						stack[stack_len++] = target;
					}
				} else if (!work_add_unique(&out, lhs, rhs, rule->rhs_count)) {
					goto fail;
				}
			}
		}
	}

	free(stack);
	free(visited);
	work_replace(grammar, &out);
	return 1;

fail:
	free(stack);
	free(visited);
	work_free(&out);
	return 0;
}

static int work_remove_useless(WorkGrammar *grammar, const Grammar *source, char **labels) {
	size_t count = grammar->nonterminal_count;
	unsigned char *generating = calloc(count, sizeof *generating);
	unsigned char *reachable  = calloc(count, sizeof *reachable);

	NonterminalId *mapping    = NULL;
	char          *new_labels = NULL;

	if (count <= SIZE_MAX / sizeof *mapping) {
		mapping = malloc(count * sizeof *mapping);
	}
	if (generating == NULL || reachable == NULL || mapping == NULL) {
		goto fail_early;
	}

	int changed;
	do {
		changed = 0;
		for (size_t i = 0; i < grammar->rule_count; ++i) {
			const GrammarRule *rule = &grammar->rules[i];
			const Symbol      *rhs  = work_rhs(grammar, rule);

			int productive = rule->rhs_count != 0;

			for (size_t j = 0; j < rule->rhs_count; ++j) {
				if (rhs[j].kind == SYMBOL_NONTERMINAL && !generating[rhs[j].id]) {
					productive = 0;
					break;
				}
			}
			if (productive && !generating[rule->lhs]) {
				generating[rule->lhs] = 1;
				changed = 1;
			}
		}
	} while (changed);

	reachable[grammar->start] = 1;
	do {
		changed = 0;
		for (size_t i = 0; i < grammar->rule_count; ++i) {
			const GrammarRule *rule = &grammar->rules[i];
			if (!reachable[rule->lhs] || !generating[rule->lhs]) {
				continue;
			}

			const Symbol *rhs = work_rhs(grammar, rule);
			int productive = 1;
			for (size_t j = 0; j < rule->rhs_count; ++j) {
				if (rhs[j].kind == SYMBOL_NONTERMINAL && !generating[rhs[j].id]) {
					productive = 0;
					break;
				}
			}
			if (!productive) {
				continue;
			}

			for (size_t j = 0; j < rule->rhs_count; ++j) {
				if (rhs[j].kind == SYMBOL_NONTERMINAL && !reachable[rhs[j].id]) {
					reachable[rhs[j].id] = 1;
					changed              = 1;
				}
			}
		}
	} while (changed);

	for (size_t i = 0; i < count; ++i) {
		mapping[i] = UINT32_MAX;
	}
	mapping[grammar->start] = 0;
	size_t kept = 1;

	for (size_t i = 0; i < count; ++i) {
		if (i == grammar->start || !generating[i] || !reachable[i]) {
			continue;
		}
		mapping[i] = (NonterminalId)kept++;
	}

	new_labels = calloc(kept, sizeof *new_labels);
	if (new_labels == NULL) {
		goto fail_early;
	}

	for (size_t i = 0; i < source->nonterminals_count; ++i) {
		if (mapping[i] != UINT32_MAX) {
			new_labels[mapping[i]] = source->nonterminals[i];
		}
	}

	WorkGrammar out = {.nonterminal_count = kept, .start = 0};
	for (size_t i = 0; i < grammar->rule_count; ++i) {
		const GrammarRule *rule = &grammar->rules[i];
		if (mapping[rule->lhs] == UINT32_MAX) {
			continue;
		}

		const Symbol *rhs = work_rhs(grammar, rule);
		Symbol mapped[2];
		int keep = rule->rhs_count != 0 && rule->rhs_count <= 2;
		for (size_t j = 0; j < rule->rhs_count && keep; ++j) {
			mapped[j] = rhs[j];
			if (rhs[j].kind == SYMBOL_NONTERMINAL) {
				if (mapping[rhs[j].id] == UINT32_MAX) {
					keep = 0;
				} else {
					mapped[j].id = mapping[rhs[j].id];
				}
			}
		}
		if (keep && !work_add_unique(&out, mapping[rule->lhs], mapped, rule->rhs_count)) {
			goto fail;
		}
	}

	free(mapping);
	free(reachable);
	free(generating);
	work_replace(grammar, &out);
	*labels = new_labels;
	return 1;

fail:
	work_free(&out);
fail_early:
	free(new_labels);
	free(mapping);
	free(reachable);
	free(generating);
	return 0;
}

static void *alloc_array(size_t count, size_t item_size) {
	if (count == 0 || count > SIZE_MAX / item_size) {
		return NULL;
	}
	return malloc(count * item_size);
}

CnfResult grammar_to_cnf(CnfGrammar *cnf, const Grammar *source) {
	assert(cnf                     != NULL);
	assert(source                  != NULL);
	assert(cnf->nonterminal_labels == NULL);
	assert(cnf->terminal_labels    == NULL);
	assert(cnf->binary_rules       == NULL);
	assert(cnf->terminal_rules     == NULL);

	if (source->nonterminals_count == 0 ||
	    !source->has_start              ||
	    source->start >= source->nonterminals_count) {
		return CNF_ERR_INVALID;
	}

	WorkGrammar work           = {0};
	char       *labels         = NULL;
	int         start_nullable = 0;
	CnfGrammar  result         = {0};
	CnfResult   status         = CNF_ERR_MEMORY;

	if (!work_from_grammar     (&work, source)                  ||
	    !work_isolate_terminals(&work, source->terminals_count) ||
	    !work_chain            (&work)                          ||
	    !work_remove_epsilon   (&work, &start_nullable)         ||
	    !work_remove_units     (&work)                          ||
	    !work_remove_useless   (&work, source, &labels)) {
		goto fail;
	}

	result.start              = work.start;
	result.start_nullable     = start_nullable;
	result.nonterminal_count  = work.nonterminal_count;
	result.nonterminal_labels = labels;
	labels = NULL;

	result.terminal_count = source->terminals_count;

	if (source->terminals_count != 0) {
		result.terminal_labels = alloc_array(source->terminals_count,
		                                     sizeof *result.terminal_labels);
		if (result.terminal_labels == NULL) {
			goto fail;
		}
		memcpy(result.terminal_labels, source->terminals,
		       source->terminals_count * sizeof *source->terminals);
	}

	for (size_t i = 0; i < work.rule_count; ++i) {
		const GrammarRule *rule = &work.rules[i];
		const Symbol      *rhs  = work_rhs(&work, rule);

		if (rule->rhs_count == 2              &&
		    rhs[0].kind == SYMBOL_NONTERMINAL &&
		    rhs[1].kind == SYMBOL_NONTERMINAL) {
			result.binary_rule_count++;
		} else if (rule->rhs_count == 1 && rhs[0].kind == SYMBOL_TERMINAL) {
			result.terminal_rule_count++;
		} else {
			status = CNF_ERR_INVALID;
			goto fail;
		}
	}

	if (result.binary_rule_count != 0) {
		result.binary_rules = alloc_array(result.binary_rule_count,
		                                  sizeof *result.binary_rules);
		if (result.binary_rules == NULL) {
			goto fail;
		}
	}
	if (result.terminal_rule_count != 0) {
		result.terminal_rules = alloc_array(result.terminal_rule_count,
		                                    sizeof *result.terminal_rules);
		if (result.terminal_rules == NULL) {
			goto fail;
		}
	}

	size_t binary   = 0;
	size_t terminal = 0;
	for (size_t i = 0; i < work.rule_count; ++i) {
		const GrammarRule *rule = &work.rules[i];
		const Symbol      *rhs  = work_rhs(&work, rule);
		if (rule->rhs_count == 2) {
			result.binary_rules[binary++] = (CnfBinaryRule){
				.lhs   = rule->lhs,
				.left  = rhs[0].id,
				.right = rhs[1].id,
			};
		} else {
			result.terminal_rules[terminal++] = (CnfTerminalRule){
				.lhs      = rule->lhs,
				.terminal = rhs[0].id,
			};
		}
	}

	work_free(&work);
	*cnf = result;
	return CNF_OK;

fail:
	cnf_free(&result);
	free(labels);
	work_free(&work);
	return status;
}

void cnf_free(CnfGrammar *cnf) {
	assert(cnf != NULL);
	free(cnf->nonterminal_labels);
	free(cnf->terminal_labels);
	free(cnf->binary_rules);
	free(cnf->terminal_rules);
	*cnf = (CnfGrammar){0};
}

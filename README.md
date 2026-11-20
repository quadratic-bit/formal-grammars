# Formal Grammars

Context-free grammar membership checking using CYK and Earley algorithms.

## Building

Requires Clang (C17), Make, and Criterion for tests.

```sh
make
```

## Running

```sh
./build/grammars < input.txt
./build/grammars --cyk < input.txt
./build/grammars --earley < input.txt
```

CYK is the default. Each input file contains a grammar followed by words to check.

Example `input.txt`:

```text
1 2 2
S
ab
S -> aSbS
S ->
S
2
aababb
aabbba
```

Output:

```text
Yes
No
```

## Arguments

- `--cyk` — use the CYK algorithm (default).
- `--earley` — use the Earley algorithm.

## Tests

```sh
make test
make coverage
make coverage-html
```

To remove build files:

```sh
make clean
```

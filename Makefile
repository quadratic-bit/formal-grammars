CC := clang

WARNFLAGS := \
	-Wall -Wextra -Wpedantic \
	-Wconversion -Wsign-conversion \
	-Wshadow -Wformat=2 -Wundef \
	-Wcast-qual -Wstrict-prototypes \
	-Wmissing-prototypes -Wswitch \
	-Wimplicit-fallthrough -Wvla

COMMON_CFLAGS := -std=c17 $(WARNFLAGS)

DEBUG_CFLAGS := -g -fno-omit-frame-pointer -fsanitize=address,undefined

RELEASE_CFLAGS := -O2 -DNDEBUG

COVFLAGS := -fprofile-instr-generate -fcoverage-mapping

CPPFLAGS := -Iinclude

SRC      := $(wildcard src/*.c)
LIB_SRC  := $(filter-out src/main.c,$(SRC))
TEST_SRC := $(wildcard tests/*.c) $(wildcard tests/helpers/*.c)

HDR := \
	$(wildcard include/grammars/*.h) \
	$(wildcard src/*.h) \
	$(wildcard tests/helpers/*.h)

ALGO_SRC := src/cnf.c src/cyk.c

.PHONY: all release test coverage coverage-show coverage-html clean

all: build/grammars

build:
	mkdir -p build

build/grammars: $(SRC) $(HDR) | build
	$(CC) $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(CPPFLAGS) $(SRC) -o $@

build/grammars-release: $(SRC) $(HDR) | build
	$(CC) $(COMMON_CFLAGS) $(RELEASE_CFLAGS) $(CPPFLAGS) $(SRC) -o $@

release: build/grammars-release

build/tests: $(LIB_SRC) $(TEST_SRC) $(HDR) | build
	$(CC) $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(CPPFLAGS) $(LIB_SRC) $(TEST_SRC) -lcriterion -o $@

test: build/tests build/grammars
	./build/tests

build/tests-cov: $(LIB_SRC) $(TEST_SRC) $(HDR) | build
	$(CC) $(COMMON_CFLAGS) $(DEBUG_CFLAGS) $(COVFLAGS) $(CPPFLAGS) \
		$(LIB_SRC) $(TEST_SRC) -lcriterion -o $@

coverage: build/tests-cov
	rm -f build/coverage-*.profraw build/coverage.profdata
	LLVM_PROFILE_FILE="build/coverage-%p.profraw" ./build/tests-cov
	llvm-profdata merge -sparse build/coverage-*.profraw -o build/coverage.profdata
	llvm-cov report ./build/tests-cov -instr-profile=build/coverage.profdata $(LIB_SRC)
	llvm-cov report ./build/tests-cov -instr-profile=build/coverage.profdata $(ALGO_SRC)

coverage-show: coverage
	llvm-cov show ./build/tests-cov -instr-profile=build/coverage.profdata \
		-show-line-counts-or-regions $(ALGO_SRC)

coverage-html: coverage
	llvm-cov show ./build/tests-cov -instr-profile=build/coverage.profdata \
		-format=html -output-dir=build/coverage-html $(ALGO_SRC)

clean:
	rm -rf build

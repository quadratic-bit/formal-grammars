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

CPPFLAGS := -Iinclude

SRC      := $(wildcard src/*.c)
LIB_SRC  := $(filter-out src/main.c,$(SRC))

.PHONY: clean

build/grammars: $(SRC) | build
	$(CC) $(COMMON_CFLAGS)  $(DEBUG_CFLAGS) $(CPPFLAGS) $(SRC) -o $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean examples

EXAMPLE_C_FILES := $(wildcard examples/*.c)
EXAMPLE_SPL_FILES := $(wildcard examples/*.spl)

EXAMPLE_C_ASM_FILES := $(EXAMPLE_C_FILES:.c=-c.s)
EXAMPLE_SPL_ASM_FILES := $(EXAMPLE_SPL_FILES:.spl=-spl.s)

EXAMPLE_EXECUTABLES := $(EXAMPLE_C_ASM_FILES:.s=) $(EXAMPLE_SPL_ASM_FILES:.s=)

# Declare .s files as secondary to prevent their automatic removal
.SECONDARY: $(EXAMPLE_C_ASM_FILES) $(EXAMPLE_SPL_ASM_FILES)

# Default target
all: spc examples

clean:
	rm -f spc $(EXAMPLE_C_ASM_FILES) $(EXAMPLE_SPL_ASM_FILES) $(EXAMPLE_EXECUTABLES)

spc: spc.c
	clang --debug -std=c99 -pedantic -Wall -Wno-gnu-case-range -o $@ $<

examples: $(EXAMPLE_EXECUTABLES)

examples/%-c: examples/%-c.s
	clang -O0 -o $@ $<

examples/%-spl: examples/%-spl.s
	clang -O0 -o $@ $<

examples/%-c.s: examples/%.c
	clang -O0 -S -o $@ $<

examples/%-spl.s: examples/%.spl spc
	./spc -o $@ $<

examples/%.spl:

.PHONY: all clean build-examples build-run-tests run-run-tests

EXAMPLE_C_FILES := $(wildcard examples/*.c)
EXAMPLE_SPL_FILES := $(wildcard examples/*.spl)
RUN_TEST_SPL_FILES := $(wildcard run-tests/*.spl)

EXAMPLE_C_ASM_FILES := $(EXAMPLE_C_FILES:.c=-c.s)
EXAMPLE_SPL_ASM_FILES := $(EXAMPLE_SPL_FILES:.spl=-spl.s)
RUN_TEST_SPL_ASM_FILES := $(RUN_TEST_SPL_FILES:.spl=-spl.s)

EXAMPLE_EXECUTABLES := $(EXAMPLE_C_ASM_FILES:.s=) $(EXAMPLE_SPL_ASM_FILES:.s=)
RUN_TEST_EXECUTABLES := $(RUN_TEST_SPL_ASM_FILES:.s=)

# Declare .s files as secondary to prevent their automatic removal
.SECONDARY: $(EXAMPLE_C_ASM_FILES) $(EXAMPLE_SPL_ASM_FILES) $(RUN_TEST_SPL_ASM_FILES)

# Default target
all: spc build-examples build-run-tests run-run-tests

clean:
	rm -f spc $(EXAMPLE_C_ASM_FILES) $(EXAMPLE_SPL_ASM_FILES) $(EXAMPLE_EXECUTABLES) \
		$(RUN_TEST_SPL_ASM_FILES) $(RUN_TEST_EXECUTABLES)

spc: spc.c
	clang --debug -std=c99 -pedantic -Wall -Wno-gnu-case-range -o $@ $<

build-examples: $(EXAMPLE_EXECUTABLES)
build-run-tests: $(RUN_TEST_EXECUTABLES)

run-run-tests: build-run-tests
	# run tests, fail on non-zero exit code
	@for test in $(RUN_TEST_EXECUTABLES); do \
		echo "Running test $$test"; \
		./$$test; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed"; \
			exit 1; \
		fi; \
	done

examples/%-c: examples/%-c.s
	clang -O0 -o $@ $<

examples/%-spl: examples/%-spl.s
	clang -O0 -o $@ $<

examples/%-c.s: examples/%.c
	clang -O0 -S -o $@ $<

examples/%-spl.s: examples/%.spl spc
	./spc -o $@ $<

examples/%.spl:

run-tests/%-spl: run-tests/%-spl.s
	clang -O0 -o $@ $<

run-tests/%-spl.s: run-tests/%.spl spc
	./spc -o $@ $<

run-tests/%.spl:

.PHONY: all clean examples test test-failure test-success

EXAMPLE_C_FILES := $(wildcard examples/*.c)
EXAMPLE_SPL_FILES := $(wildcard examples/*.spl)
TEST_FAILURE_SPL_FILES := $(wildcard tests/failure/*.spl)
TEST_SUCCESS_SPL_FILES := $(wildcard tests/success/*.spl)

EXAMPLE_C_ASM_FILES := $(EXAMPLE_C_FILES:.c=-c.s)
EXAMPLE_SPL_ASM_FILES := $(EXAMPLE_SPL_FILES:.spl=-spl.s)
TEST_SUCCESS_SPL_ASM_FILES := $(TEST_SUCCESS_SPL_FILES:.spl=-spl.s)

EXAMPLE_EXECUTABLES := $(EXAMPLE_C_ASM_FILES:.s=) $(EXAMPLE_SPL_ASM_FILES:.s=)
TEST_SUCCESS_EXECUTABLES := $(TEST_SUCCESS_SPL_ASM_FILES:.s=)

# Declare .s files as secondary to prevent their automatic removal
.SECONDARY: $(EXAMPLE_C_ASM_FILES) $(EXAMPLE_SPL_ASM_FILES) \
			$(TEST_SUCCESS_SPL_ASM_FILES)

# Default target
all: spc examples test

clean:
	git clean -dfX --exclude="!/.vscode"

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
	./spc -g -o $@ $<

examples/%.spl:


test: test-failure test-success


test-failure:
	@echo "Running failure tests..."
	@FAIL=0; \
	for SPL in tests/failure/*.spl; do \
		TMP_OUT=$$(mktemp); \
		OUT=$$(echo "$$SPL" | sed 's/\.[^.]*$$/.txt/'); \
		./spc -g -o $$(mktemp) "$$SPL" >"$$TMP_OUT" 2>&1; \
		if cmp -s "$$TMP_OUT" "$$OUT"; then \
			rm "$$TMP_OUT"; \
		else \
			echo "FAIL: writing expected output to '$$OUT'"; \
			mv "$$TMP_OUT" "$$OUT"; \
			FAIL=1; \
		fi; \
	done; \
	if [ "$$FAIL" -eq 1 ]; then \
		echo "One or more tests failed."; \
		exit 1; \
	else \
		echo "Passed."; \
	fi

tests/failure/%.spl:


test-success: $(TEST_SUCCESS_EXECUTABLES)
	@for test in $(TEST_SUCCESS_EXECUTABLES); do \
		echo "Running $$test..."; \
		./$$test; \
		if [ $$? -ne 0 ]; then \
			echo "Test $$test failed"; \
			exit 1; \
		fi; \
	done

tests/success/%-spl: tests/success/%-spl.o
	clang -O0 --debug -o $@ $<

tests/success/%-spl.o: tests/success/%-spl.s
	clang -c -O0 --debug -o $@ $<

tests/success/%-spl.s: tests/success/%.spl spc
	./spc -g -o $@ $<

tests/success/%.spl:

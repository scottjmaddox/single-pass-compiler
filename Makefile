.PHONY: all clean test test-failure test-success

TEST_FAILURE_SPL_FILES := $(wildcard tests/failure/*.spl)
TEST_SUCCESS_SPL_FILES := $(wildcard tests/success/*.spl)

TEST_SUCCESS_SPL_ASM_FILES := $(TEST_SUCCESS_SPL_FILES:.spl=.s)

TEST_SUCCESS_EXECUTABLES := $(TEST_SUCCESS_SPL_ASM_FILES:.s=)

# Declare .s files as secondary to prevent their automatic removal
.SECONDARY: $(TEST_SUCCESS_SPL_ASM_FILES)

# Default target
all: spc test

clean:
	git clean -dfX --exclude="!/.vscode"

spc: spc.c
	clang --debug -std=c11 -pedantic -Wall -Wno-gnu-case-range -o $@ $<


test: test-failure test-success


test-failure:
	@echo "Running failure tests..."
	@FAIL=0; \
	for SPL in $(TEST_FAILURE_SPL_FILES); do \
		TMP_OUT=$$(mktemp); \
		OUT=$$(echo "$$SPL" | sed 's/\.[^.]*$$/.txt/'); \
		./spc -o $$(mktemp) "$$SPL" >"$$TMP_OUT" 2>&1; \
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

tests/success/%: tests/success/%.s
	clang -O0 --debug -o $@ $<

tests/success/%.s: tests/success/%.spl spc
	./spc -o $@ $<

tests/success/%.spl:

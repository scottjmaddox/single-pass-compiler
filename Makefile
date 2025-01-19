.PHONY: all clean test test-failure test-success

TEST_ASSEMBLY_SPL_FILES := $(wildcard tests/assembly/*.spl)
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
	clang --debug -std=c11 -pedantic -Wall -Wno-gnu-case-range \
		-fprofile-instr-generate -fcoverage-mapping -mmacosx-version-min=14.7 \
		-o $@ $<


test: test-assembly test-failure test-success
	@llvm-profdata merge -o tests.profdata tests/*/*.profraw
	llvm-cov show ./spc -instr-profile=tests.profdata -format=html -output-dir=coverage


test-assembly:
	@echo "Running assembly tests..."
	@FAIL=0; \
	for SPL in $(TEST_ASSEMBLY_SPL_FILES); do \
		TMP_OUT=$$(mktemp); \
		PROF_OUT="$$(echo "$$SPL" | sed 's/\.[^.]*$$/.profraw/')"; \
		OUT="$$(echo "$$SPL" | sed 's/\.[^.]*$$/.s/')"; \
		LLVM_PROFILE_FILE="$$PROF_OUT.profraw" ./spc -o $$TMP_OUT "$$SPL"; \
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


test-failure:
	@echo "Running failure tests..."
	@FAIL=0; \
	for SPL in $(TEST_FAILURE_SPL_FILES); do \
		TMP_OUT=$$(mktemp); \
		PROF_OUT="$$(echo "$$SPL" | sed 's/\.[^.]*$$/.profraw/')"; \
		OUT="$$(echo "$$SPL" | sed 's/\.[^.]*$$/.txt/')"; \
		LLVM_PROFILE_FILE="$$PROF_OUT.profraw" ./spc -o $$(mktemp) "$$SPL" >"$$TMP_OUT" 2>&1; \
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
	LLVM_PROFILE_FILE="$$(echo "$<" | sed 's/\.[^.]*$$/.profraw/')" ./spc -o $@ $<

tests/success/%.spl:

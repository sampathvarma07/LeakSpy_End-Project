#!/usr/bin/env bash

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT

passed=0
failed=0

run_test() {
    local name="$1"
    local expected_blocks="$2"
    local expected_bytes="$3"
    local output="$TEMP_DIR/$name.log"

    echo
    echo "Running $name..."

    if ! env LD_PRELOAD="$ROOT/leakspy.so" \
        LEAKSPY_MAPS_PATH="$TEMP_DIR/$name.maps" \
        "$ROOT/tests/$name" >"$output" 2>&1; then
        echo "FAIL: Program exited unsuccessfully."
        cat "$output"
        failed=$((failed + 1))
        return
    fi

    if grep -Fxq "Leaked blocks: $expected_blocks" "$output" &&
       grep -Fxq "Leaked bytes: $expected_bytes" "$output"; then
        echo "PASS: $name"
        echo "  Leaked blocks: $expected_blocks"
        echo "  Leaked bytes: $expected_bytes"
        passed=$((passed + 1))
    else
        echo "FAIL: Unexpected leak report for $name"
        cat "$output"
        failed=$((failed + 1))
    fi
}

if [[ ! -f "$ROOT/leakspy.so" ]]; then
    echo "ERROR: leakspy.so is missing. Run make first."
    exit 2
fi

for name in test_leak test_normal test_calloc test_realloc test_realloc_fail test_calloc_overflow test_zero_alloc; do
    if [[ ! -x "$ROOT/tests/$name" ]]; then
        echo "ERROR: $ROOT/tests/$name is missing. Run make first."
        exit 2
    fi
done

run_test test_leak 1 40
run_test test_normal 0 0
run_test test_calloc 1 40
run_test test_realloc 1 40
run_test test_realloc_fail 1 40
run_test test_calloc_overflow 0 0
run_test test_zero_alloc 0 0

echo
echo "========== Regression Test Summary =========="
echo "Passed: $passed"
echo "Failed: $failed"
echo "============================================="

if (( failed > 0 )); then
    exit 1
fi

echo "All regression tests passed."

#!/bin/bash

total=0
failed=0

function run_test() {
    ((total++))
    echo "Running test $1..."
    ./sleepdart --test "$1" || ((failed++))
}

for test in tests/*/; do
    if ! echo "$test" | grep -q "optional" || [[ "$1" = "all" ]]; then
        run_test "$test"
    fi
done

if [[ "$failed" = 0 ]]; then
    echo "Passed all $total tests!"
else
    echo "Failed $failed out of $total tests."
    exit "$failed"
fi

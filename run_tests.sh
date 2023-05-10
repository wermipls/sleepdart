#!/bin/bash

total=0
failed=0

for test in tests/*/; do
    ((total++))
    echo "Running test $test..."
    ./sleepdart --test "$test" || ((failed++))
done

if [[ "$failed" = 0 ]]; then
    echo "Passed all $total tests!"
else
    echo "Failed $failed out of $total tests."
    exit "$failed"
fi

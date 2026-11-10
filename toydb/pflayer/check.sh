#!/bin/sh
# Build only copied sources, and keep all generated files outside the checkout.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
check_dir=$(mktemp -d "${TMPDIR:-/tmp}/toydb-check.XXXXXX")
printf 'Build, test data and logs: %s\n' "$check_dir"
mkdir "$check_dir/pflayer" "$check_dir/amlayer" "$check_dir/run"
cp "$source_dir"/pflayer/*.[ch] "$source_dir/pflayer/Makefile" "$check_dir/pflayer/"
cp "$source_dir"/amlayer/*.[ch] "$source_dir/amlayer/makefile" "$check_dir/amlayer/"
make -B -C "$check_dir/pflayer" tests > "$check_dir/pf-build.log" 2>&1 || {
    cat "$check_dir/pf-build.log"; exit 1;
}
make -B -C "$check_dir/amlayer" amlayer.o > "$check_dir/am-build.log" 2>&1 || {
    cat "$check_dir/am-build.log"; exit 1;
}
printf 'PASS: PF and AM rebuilt from source\n'
cd "$check_dir/run"
for test in testhash testpf; do
    "$check_dir/pflayer/$test" > "$check_dir/$test.log" 2>&1 || {
        cat "$check_dir/$test.log"; exit 1;
    }
    printf 'PASS: %s\n' "$test"
done
for scenario in default two one large allocate; do
    "$check_dir/pflayer/testbufsize" "$scenario" > "$check_dir/$scenario.log" 2>&1 || {
        cat "$check_dir/$scenario.log"; exit 1;
    }
    cat "$check_dir/$scenario.log"
done

for policy in 0 1; do
    for scenario in replacement allocation reuse fixed dirty mixed; do
        "$check_dir/pflayer/testphase1" "$scenario" "$policy"
    done
done
for scenario in replacement stats invalid; do
    "$check_dir/pflayer/testphase1" "$scenario" -1
done

"$check_dir/pflayer/testiofail"

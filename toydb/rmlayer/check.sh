#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/toydb-rm-check.XXXXXX")
printf 'Phase 2 build, data and logs: %s\n' "$work"
mkdir "$work/pflayer" "$work/rmlayer"
cp "$root"/pflayer/*.[ch] "$root/pflayer/Makefile" "$work/pflayer/"
cp "$root"/rmlayer/*.[ch] "$root/rmlayer/Makefile" "$work/rmlayer/"
make -B -C "$work/rmlayer" all test_records test_student test_failures benchmark > "$work/build.log" 2>&1 || { cat "$work/build.log"; exit 1; }
for capacity in 1 3; do
    for policy in 0 1; do
        run="$work/basic-$capacity-$policy"
        mkdir "$run"
        (cd "$run" && "$work/rmlayer/test_rm" "$capacity" "$policy")
        for scenario in boundaries random fixed; do
            run="$work/$scenario-$capacity-$policy"
            mkdir "$run"
            (cd "$run" && "$work/rmlayer/test_records" "$capacity" "$policy" "$scenario")
        done
    done
done

mkdir "$work/student" "$work/failures"
(cd "$work/student" && "$work/rmlayer/test_student")
(cd "$work/failures" && "$work/rmlayer/test_failures")
python3 "$root/rmlayer/test_import.py" "$work/rmlayer/benchmark" "$work"

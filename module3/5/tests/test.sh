#!/bin/bash
# Интеграционный тест: производитель и потребители через настоящее имя /eltex_pc.
# Если в это время запущен свой производитель, тест упадёт на создании.
P=$(realpath "$(dirname "$0")/../build/producer")
C=$(realpath "$(dirname "$0")/../build/consumer")
SHM=/dev/shm/eltex_pc
SEM=/dev/shm/sem.eltex_pc
T=$(mktemp -d)
pass=0; fail=0
rc=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

cleanup() {
    jobs -p | xargs -r kill -CONT 2>/dev/null
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.3
    rm -rf "$T"
}
trap cleanup EXIT

run()     { timeout 10 "$@" </dev/null >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
gone()    { [ ! -e "$SHM" ] && [ ! -e "$SEM" ] && ok "$1" || bad "$1"; }
sets()    { cat "$@" | grep -o 'set of [0-9]* at offset [0-9]*' | sort; }

wait_for() {
    local i
    for i in $(seq 100); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
    return 1
}
wait_exit() {
    local i
    for i in $(seq 150); do kill -0 "$1" 2>/dev/null || return 0; sleep 0.1; done
    return 1
}
exits_with() {
    if wait_exit "$2"; then
        wait "$2"; rc=$?
        [ "$rc" -eq "$3" ] && ok "$1" || bad "$1 (rc=$rc, want $3)"
    else
        bad "$1 (process still running)"
    fi
}

echo "== invalid usage =="
run "$P" abc;      want_rc "producer: size is not a number" 2
run "$P" 0;        want_rc "producer: zero size" 2
run "$P" 1 2;      want_rc "producer: two arguments" 2
run "$C" x;        want_rc "consumer: argument" 2

echo "== consumer without producer =="
run "$C";          want_rc "consumer fails" 1
grep -q "producer running" "$T/err" && ok "consumer explains why" || bad "consumer explains why"

echo "== producer and two consumers =="
"$P" 1024 >"$T/p.out" 2>&1 & PP=$!
wait_for "$T/p.out" "bytes" || bad "producer started"
[ -e "$SHM" ] && [ -e "$SEM" ] && ok "objects visible in /dev/shm" || bad "objects visible in /dev/shm"
run "$P";          want_rc "second producer fails" 1
"$C" >"$T/c1.out" 2>&1 & C1=$!
"$C" >"$T/c2.out" 2>&1 & C2=$!
exits_with "producer exits after all sets are processed" "$PP" 0
exits_with "consumer 1 exits" "$C1" 0
exits_with "consumer 2 exits" "$C2" 0
grep -q "all sets processed" "$T/p.out" && ok "producer waited for processing" || bad "producer waited for processing"
sets "$T/p.out" >"$T/made"
sets "$T/c1.out" "$T/c2.out" >"$T/taken"
[ -s "$T/made" ] && cmp -s "$T/made" "$T/taken" && ok "every set processed exactly once" \
    || { bad "every set processed exactly once"; diff "$T/made" "$T/taken"; }
grep -q 'set of' "$T/c1.out" && grep -q 'set of' "$T/c2.out" && ok "both consumers got work" || bad "both consumers got work"
gone "names removed from /dev/shm"

echo "== producer interrupted while waiting =="
"$P" 256 >"$T/p.out" 2>&1 & PP=$!
wait_for "$T/p.out" "memory is full" || bad "producer filled memory"
"$C" >"$T/c1.out" 2>&1 & C1=$!
wait_for "$T/c1.out" "set of" || bad "consumer started"
kill -STOP "$C1"
kill -INT "$PP"
exits_with "producer exits on SIGINT" "$PP" 0
gone "names removed at once, though a consumer is attached"
kill -CONT "$C1"
exits_with "consumer exits" "$C1" 0
sets "$T/p.out" >"$T/made"
sets "$T/c1.out" >"$T/taken"
cmp -s "$T/made" "$T/taken" && ok "consumer finished the remaining sets after unlink" \
    || bad "consumer finished the remaining sets after unlink"

echo "== producer interrupted while generating =="
"$P" 100000 >"$T/p.out" 2>&1 & PP=$!
wait_for "$T/p.out" "set of" || bad "producer started generating"
"$C" >"$T/c1.out" 2>&1 & C1=$!
wait_for "$T/c1.out" "set of" || bad "consumer started"
kill -INT "$PP"
exits_with "producer exits on SIGINT" "$PP" 0
grep -q "memory is full" "$T/p.out" && bad "memory was not full yet" || ok "memory was not full yet"
exits_with "consumer exits thanks to done" "$C1" 0
gone "names removed"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

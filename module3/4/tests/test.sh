#!/bin/bash
# Интеграционный тест: производитель и потребители через настоящий ключ ftok("/tmp", 'D').
# Если в это время запущен свой производитель, тест упадёт на создании.
P=$(realpath "$(dirname "$0")/../build/producer")
C=$(realpath "$(dirname "$0")/../build/consumer")
T=$(mktemp -d)
pass=0; fail=0
rc=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

cleanup() {
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.3
    rm -rf "$T"
}
trap cleanup EXIT

run()     { timeout 10 "$@" </dev/null >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }

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
# id из строки "producer: shm N, sem M" и проверка, что ядро их удалило
ids()  { SHM=$(awk '/shm/{print $3+0; exit}' "$1"); SEM=$(awk '/sem/{print $5+0; exit}' "$1"); }
# ipcs -i возвращает 0 и для несуществующего id, поэтому ищем id во второй колонке списка
has()  { ipcs "$1" | awk '{print $2}' | grep -qx "$2"; }
gone() { ! has -m "$SHM" && ! has -s "$SEM" && ok "$1" || bad "$1"; }

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
wait_for "$T/p.out" "shm" || bad "producer started"
ids "$T/p.out"
run "$P";          want_rc "second producer fails" 1
"$C" >"$T/c1.out" 2>&1 & C1=$!
"$C" >"$T/c2.out" 2>&1 & C2=$!
exits_with "producer exits after all sets are processed" "$PP" 0
exits_with "consumer 1 exits" "$C1" 0
exits_with "consumer 2 exits" "$C2" 0
grep -q "memory is full" "$T/p.out" && ok "producer filled memory" || bad "producer filled memory"
grep -q "all sets processed" "$T/p.out" && ok "producer waited for processing" || bad "producer waited for processing"
# Каждый набор обработан ровно один раз: смещения у производителя и у потребителей совпадают
grep -o 'set of [0-9]* at offset [0-9]*' "$T/p.out" | sort >"$T/made"
cat "$T/c1.out" "$T/c2.out" | grep -o 'set of [0-9]* at offset [0-9]*' | sort >"$T/taken"
[ -s "$T/made" ] && cmp -s "$T/made" "$T/taken" && ok "every set processed exactly once" \
    || { bad "every set processed exactly once"; diff "$T/made" "$T/taken"; }
grep -q 'set of' "$T/c1.out" && grep -q 'set of' "$T/c2.out" && ok "both consumers got work" || bad "both consumers got work"
gone "shared memory and semaphore removed"

echo "== producer interrupted while nobody consumes =="
"$P" 256 >"$T/p.out" 2>&1 & PP=$!
wait_for "$T/p.out" "memory is full" || bad "producer filled memory"
ids "$T/p.out"
"$C" >"$T/c1.out" 2>&1 & C1=$!
wait_for "$T/c1.out" "set of" || bad "consumer started"
kill -STOP "$C1"
kill -INT "$PP"
exits_with "producer exits on SIGINT" "$PP" 0
! has -s "$SEM" && ok "semaphore removed at once" || bad "semaphore removed at once"
# Сегмент только помечен к удалению (dest), пока потребитель к нему подключён
ipcs -m | awk -v id="$SHM" '$2 == id && /dest/' | grep -q . && ok "segment marked dest while attached" \
    || bad "segment marked dest while attached"
kill -CONT "$C1"
exits_with "consumer exits when objects are removed" "$C1" 0
gone "segment removed after last detach"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Интеграционный тест: три клиента на одной машине, рассылка по 127.255.255.255 (не выходит наружу).
# Ввод клиентам подаём через именованные каналы в fd 3, 4, 5.
BIN=$(realpath "$(dirname "$0")/../build/chat")
ADDR=127.255.255.255
PORT=50996
T=$(mktemp -d)
pass=0; fail=0
rc=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

cleanup() {
    exec 3>&- 4>&- 5>&- 2>/dev/null
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.3
    rm -rf "$T"
}
trap cleanup EXIT

run()     { timeout 5 "$BIN" "$@" </dev/null >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
has()     { wait_for "$2" "$3" && ok "$1" || bad "$1"; }
hasnt()   { sleep 0.3; ! grep -q -- "$3" "$2" && ok "$1" || bad "$1"; }

wait_for() {
    local i
    for i in $(seq 50); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
    return 1
}
wait_exit() {
    local i
    for i in $(seq 50); do kill -0 "$1" 2>/dev/null || return 0; sleep 0.1; done
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

# start ИМЯ FD: клиент читает свой канал, закрываем ему чужие fd, иначе каналы не увидят конца ввода
start() {
    mkfifo "$T/$1.in"
    "$BIN" "$1" "$ADDR" "$PORT" <"$T/$1.in" >"$T/$1.out" 2>&1 3>&- 4>&- 5>&- &
    eval "PID_$1=$!"
    eval "exec $2>\"$T/$1.in\""
    wait_for "$T/$1.out" "^chat: $1" || bad "$1 started"
}

echo "== invalid usage =="
run;                          want_rc "no arguments" 2
run "two words";              want_rc "name with a space" 2
run bob 300.1.1.1;            want_rc "bad address" 2
run bob "$ADDR" 0;            want_rc "port 0" 2
run bob "$ADDR" "$PORT" x;    want_rc "too many arguments" 2

echo "== join =="
start alice 3
start bob 4
has "alice sees bob joined" "$T/alice.out" "\*\*\* bob joined (127.0.0.1)"
start carol 5
has "alice sees carol joined" "$T/alice.out" "\*\*\* carol joined"
has "bob sees carol joined" "$T/bob.out" "\*\*\* carol joined"
hasnt "bob did not see own join" "$T/bob.out" "bob joined"

echo "== messages =="
echo "hello everyone" >&3
has "bob got alice's message" "$T/bob.out" "^alice: hello everyone$"
has "carol got alice's message" "$T/carol.out" "^alice: hello everyone$"
hasnt "alice did not get her own echo" "$T/alice.out" "alice: hello"
printf 'one\ntwo\n' >&4
has "alice got bob's second line" "$T/alice.out" "^bob: two$"
grep -A1 "^bob: one$" "$T/carol.out" | grep -q "^bob: two$" && ok "lines arrive in order" || bad "lines arrive in order"

echo "== leave =="
exec 4>&-
exits_with "bob exits on end of input" "$PID_bob" 0
has "alice sees bob left" "$T/alice.out" "\*\*\* bob left"
has "carol sees bob left" "$T/carol.out" "\*\*\* bob left"
kill -INT "$PID_carol"
exits_with "carol exits on SIGINT" "$PID_carol" 0
has "alice sees carol left after SIGINT" "$T/alice.out" "\*\*\* carol left"
kill -TERM "$PID_alice"
exits_with "alice exits on SIGTERM" "$PID_alice" 0
exec 3>&- 5>&-

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

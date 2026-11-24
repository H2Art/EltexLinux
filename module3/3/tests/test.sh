#!/bin/bash
# Интеграционный тест: два процесса чата, ввод подаём через именованные каналы.
BIN=$(realpath "$(dirname "$0")/../build/chat")
T=$(mktemp -d)
NAME="chat_test_$$"
pass=0; fail=0
rc=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

cleanup() {
    exec 3>&- 4>&- 2>/dev/null
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.3
    rm -f "/dev/mqueue/${NAME}_1" "/dev/mqueue/${NAME}_2"
    rm -rf "$T"
}
trap cleanup EXIT

run()     { timeout 10 "$BIN" "$@" </dev/null >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
same()    { printf '%b' "$3" | cmp -s - "$2" && ok "$1" || { bad "$1"; echo "--- got:"; cat "$2"; }; }
gone()    { [ ! -e "/dev/mqueue/${NAME}_1" ] && [ ! -e "/dev/mqueue/${NAME}_2" ] && ok "$1" || bad "$1"; }

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

# Запускает пару: A создаёт очереди, B присоединяется. Ввод A в fd 3, ввод B в fd 4.
# Запускаемым процессам закрываем 3 и 4: иначе B унаследует конец записи в канал A,
# и A не увидит конца ввода, пока жив B.
start_pair() {
    rm -f "$T"/a.* "$T"/b.*
    mkfifo "$T/a.in" "$T/b.in"
    "$BIN" "$NAME" <"$T/a.in" >"$T/a.out" 2>&1 3>&- 4>&- & A=$!
    exec 3>"$T/a.in"
    wait_for "$T/a.out" "created" || bad "A created queues"
    "$BIN" "$NAME" <"$T/b.in" >"$T/b.out" 2>&1 3>&- 4>&- & B=$!
    exec 4>"$T/b.in"
    wait_for "$T/b.out" "joined" || bad "B joined"
}

echo "== invalid usage =="
run;                 want_rc "no arguments" 2
run a b;             want_rc "two arguments" 2
run "";              want_rc "empty name" 2
run "a/b";           want_rc "slash inside name" 2

echo "== roles and exchange =="
start_pair
ok "first process created queues, second joined"
echo "hello from A" >&3
wait_for "$T/b.out" "peer: hello from A" && ok "A -> B" || bad "A -> B"
echo "hi from B" >&4
wait_for "$T/a.out" "peer: hi from B" && ok "B -> A" || bad "B -> A"

echo "== joiner leaves (end of input) =="
exec 4>&-
exits_with "B exits on end of input" "$B" 0
wait_for "$T/a.out" "peer left the chat" && ok "A notified that B left" || bad "A notified that B left"
exits_with "A exits after B left" "$A" 0
exec 3>&-
gone "creator removed both queues"

echo "== creator leaves on SIGINT =="
start_pair
kill -INT "$A"
exits_with "A exits on SIGINT" "$A" 0
wait_for "$T/b.out" "peer left the chat" && ok "B notified about SIGINT of A" || bad "B notified about SIGINT of A"
exits_with "B exits after A left" "$B" 0
exec 3>&- 4>&-
gone "queues removed after creator left first"

echo "== bye does not overtake unread messages =="
start_pair
printf 'one\ntwo\nthree\n' >&3
exec 3>&-
exits_with "A sends three lines and exits" "$A" 0
exits_with "B exits after reading everything" "$B" 0
exec 4>&-
tail -n +2 "$T/b.out" >"$T/b.msgs"
same "B got all messages, then the bye" "$T/b.msgs" 'peer: one\npeer: two\npeer: three\npeer left the chat\n'
gone "queues removed"

echo "== messages wait for a peer that is not there yet =="
rm -f "$T"/a.*; mkfifo "$T/a.in"
"$BIN" "$NAME" <"$T/a.in" >"$T/a.out" 2>&1 3>&- 4>&- & A=$!
exec 3>"$T/a.in"
wait_for "$T/a.out" "created" || bad "A created queues"
echo "sent before B came" >&3
sleep 0.3
"$BIN" "$NAME" </dev/null >"$T/b.out" 2>&1 3>&- 4>&- & B=$!
exits_with "B with empty input exits" "$B" 0
grep -q "peer: sent before B came" "$T/b.out" && ok "B got the message sent before it joined" || bad "B got the message sent before it joined"
exits_with "A exits after B left" "$A" 0
exec 3>&-
gone "queues removed"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Интеграционный тест: сервер и клиенты на 127.0.0.1, ввод клиентов через именованные каналы.
S=$(realpath "$(dirname "$0")/../build/server")
C=$(realpath "$(dirname "$0")/../build/client")
PORT=$((52000 + $$ % 1000))
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

run()     { timeout 10 "$@" </dev/null >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
has()     { wait_for "$2" "$3" && ok "$1" || bad "$1"; }

wait_for() {
    local i
    for i in $(seq 100); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
    return 1
}
wait_exit() {
    local i
    for i in $(seq 100); do kill -0 "$1" 2>/dev/null || return 0; sleep 0.1; done
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

start_server() {
    "$S" "$PORT" >"$T/s.out" 2>&1 3>&- 4>&- 5>&- & SP=$!
    wait_for "$T/s.out" "listening" || bad "server started"
}
# Клиент в своей папке (туда же сохраняет файлы), ввод из канала: start_client имя fd
start_client() {
    mkdir -p "$T/$1"; mkfifo "$T/$1.in"
    (cd "$T/$1" && exec "$C" "$1" 127.0.0.1 "$PORT" <"$T/$1.in" >"$T/$1.out" 2>&1 3>&- 4>&- 5>&-) &
    eval "exec $2>\"$T/$1.in\""
    wait_for "$T/$1.out" "online" || bad "$1 connected"
}

echo "== invalid usage =="
run "$S" 0;                       want_rc "server: port 0" 2
run "$S" abc;                     want_rc "server: port not a number" 2
run "$C";                         want_rc "client: no arguments" 2
run "$C" "a b" 127.0.0.1;         want_rc "client: name with a space" 2
run "$C" bob 127.0.0.1 "$PORT";   want_rc "client: no server" 1

echo "== join and list of users =="
start_server
start_client alice 3; A=$!
has "first client sees nobody else" "$T/alice.out" "online: nobody else"
start_client bob 4; B=$!
has "second client gets the list" "$T/bob.out" "online: alice"
has "alice sees bob joined" "$T/alice.out" "\*\*\* bob joined"
run "$C" alice 127.0.0.1 "$PORT"; want_rc "duplicate name rejected" 1
grep -q "name is taken" "$T/err" && ok "client explains the rejection" || bad "client explains the rejection"

echo "== text =="
echo "hello bob" >&3
has "bob gets alice's text" "$T/bob.out" "^alice: hello bob$"
echo "hi alice" >&4
has "alice gets bob's text" "$T/alice.out" "^bob: hi alice$"
grep -q "^alice:" "$T/alice.out" && bad "sender does not get own text" || ok "sender does not get own text"

echo "== files =="
head -c 300000 /dev/urandom >"$T/alice/pic.bin"
echo "/file pic.bin" >&3
has "bob saved the file" "$T/bob.out" "saved as recv_alice_pic.bin"
cmp -s "$T/alice/pic.bin" "$T/bob/recv_alice_pic.bin" && ok "file arrived intact" || bad "file arrived intact"
echo "/file pic.bin" >&3
has "same file again does not overwrite (O_EXCL)" "$T/bob.out" "not saved: File exists"
head -c $((1048576 + 1)) /dev/zero >"$T/alice/huge.bin"
echo "/file huge.bin" >&3
has "file over the limit is refused" "$T/alice.out" "cannot send 'huge.bin'"

echo "== slow client does not stall the server =="
# Представился и ничего не читает: его очередь растёт, а остальные должны получать сообщения
exec 5<>"/dev/tcp/127.0.0.1/$PORT"
printf 'H\x00\x00\x00\x04slow' >&5
has "slow client joined" "$T/alice.out" "\*\*\* slow joined"
head -c 1048576 /dev/urandom >"$T/alice/big.bin"
for i in $(seq 12); do echo "/file big.bin" >&3; done
echo "after the big files" >&3
has "bob still gets messages" "$T/bob.out" "^alice: after the big files$"
has "server drops the client that does not read" "$T/s.out" "dropping slow client slow"
has "others are told the slow client left" "$T/bob.out" "\*\*\* slow left"
exec 5>&-

echo "== leave and server stop =="
exec 4>&-
exits_with "bob exits on end of input" "$B" 0
has "alice sees bob left" "$T/alice.out" "\*\*\* bob left"
kill -INT "$SP"
exits_with "server exits on SIGINT" "$SP" 0
has "alice is told the server closed" "$T/alice.out" "server closed the connection"
exits_with "alice exits" "$A" 0
exec 3>&-
"$S" "$PORT" >"$T/s.out" 2>&1 3>&- 4>&- 5>&- & SP=$!
has "server restarts on the same port at once (SO_REUSEADDR)" "$T/s.out" "listening"
kill -INT "$SP"; exits_with "restarted server exits" "$SP" 0

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Интеграционный тест. RAW-сокетам нужен CAP_NET_RAW: перезапускаемся в своём user + network namespace.
S=$(realpath "$(dirname "$0")/../build/server")
C=$(realpath "$(dirname "$0")/../build/client")
pass=0; fail=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }
want_rc() { timeout 3 "$@" >/dev/null 2>&1; echo $?; }   # timeout: зависание — тоже ошибка (rc 124)

if [ -z "$IN_NETNS" ]; then
    echo "== invalid usage =="
    check "server: bad port" '[ $(want_rc "$S" -p 0) -eq 2 ]'
    check "server: extra argument" '[ $(want_rc "$S" extra) -eq 2 ]'
    check "client: no server ip" '[ $(want_rc "$C") -eq 2 ]'
    check "client: not an ip" '[ $(want_rc "$C" localhost) -eq 2 ]'
    check "client: -k 0" '[ $(want_rc "$C" -k 0 127.0.0.1) -eq 2 ]'
    if [ "$(id -u)" -ne 0 ]; then
        err=$("$S" 2>&1); rc=$?
        check "without CAP_NET_RAW: rc 1 and a hint" '[ $rc -eq 1 ] && [[ "$err" == *CAP_NET_RAW* ]]'
    fi
    IN_NETNS=1 PASS0=$pass FAIL0=$fail exec unshare -rn "$0"
fi
pass=$PASS0; fail=$FAIL0
T=$(mktemp -d)
cleanup() { exec 3>&- 4>&- 5>&- 2>/dev/null; jobs -p | xargs -r kill -KILL 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT
ip link set lo up

wait_for() {    # wait_for файл шаблон
    local i
    for i in $(seq 100); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.05; done
    return 1
}
has() { wait_for "$2" "$3" && ok "$1" || bad "$1"; }
wait_rc() {     # wait_rc pid: дождаться выхода, код в rc
    local i
    for i in $(seq 100); do kill -0 "$1" 2>/dev/null || break; sleep 0.05; done
    wait "$1"; rc=$?
}
# start_client имя fd [опции клиента]: ввод из FIFO на дескрипторе fd
start_client() {
    local name=$1 fd=$2; shift 2
    mkfifo "$T/$name.in"
    "$C" "$@" 127.0.0.1 <"$T/$name.in" >"$T/$name.out" 2>"$T/$name.err" 3>&- 4>&- 5>&- &
    eval "exec $fd>\"$T/$name.in\""
    wait_for "$T/$name.err" "client: port" || bad "$name started"
}

echo "== server =="
tcpdump -Z root -i lo -nn -l icmp >"$T/icmp" 2>/dev/null & TD=$!
"$S" -T 3 >"$T/s.out" 2>&1 3>&- 4>&- 5>&- & SP=$!
has "server listens" "$T/s.out" "listening on 51500"
check "second server on the same port: rc 1" '[ $(want_rc "$S") -eq 1 ]'

echo "== numbering per client (example from the task) =="
start_client c1 3 -b 40001; C1=$!
start_client c2 4 -b 40002; C2=$!
echo WAAAAAAAGH >&3; has "c1: WAAAAAAAGH 1" "$T/c1.out" "^WAAAAAAAGH 1$"
echo ping >&3;       has "c1: ping 2" "$T/c1.out" "^ping 2$"
echo ping >&4;       has "c2 has its own counter: ping 1" "$T/c2.out" "^ping 1$"
echo WAAAAAAAGH >&3; has "c1: WAAAAAAAGH 3" "$T/c1.out" "^WAAAAAAAGH 3$"
check "client prints only replies, not its own packets" "[ \$(wc -l <'$T/c1.out') -eq 3 ]"
python3 -I -c "
import socket; s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(1)
s.sendto(b'plain', ('127.0.0.1', 51500)); print(s.recv(100).decode())" >"$T/py.out" 2>&1
check "ordinary UDP socket gets the reply (header and checksum 0 are valid)" "grep -qx 'plain 1' '$T/py.out'"

echo "== close resets the counter =="
exec 3>&-; wait_rc "$C1"
check "c1 exits on end of input (rc=$rc)" "[ $rc -eq 0 ]"
has "server got the close from c1" "$T/s.out" "127.0.0.1:40001 closed"
start_client c1b 3 -b 40001; C1=$!
echo again >&3; has "same ip:port starts from 1 again" "$T/c1b.out" "^again 1$"
kill -INT "$C2"; wait_rc "$C2"
check "c2 exits on SIGINT (rc=$rc)" "[ $rc -eq 0 ]"
has "SIGINT also sends the close" "$T/s.out" "127.0.0.1:40002 closed"
kill -TERM "$C1"; wait_rc "$C1"
has "SIGTERM also sends the close" "$T/s.out" "127.0.0.1:40001 closed"
check "close came twice for 40001 (Ctrl+D, then SIGTERM)" "[ \$(grep -c '40001 closed' '$T/s.out') -eq 2 ]"

echo "== heartbeat and expiry (server -T 3) =="
start_client alive 3 -b 40003 -k 1; CA=$!
start_client dead 4 -b 40004; CD=$!
echo one >&3; echo one >&4
has "both numbered" "$T/dead.out" "^one 1$"
kill -KILL "$CD"; wait "$CD" 2>/dev/null    # без закрытия: сервер узнает только по тишине
sleep 5
echo two >&3
has "client with heartbeat keeps its counter after 5 s of silence" "$T/alive.out" "^two 2$"
check "it was not expired" "! grep -q '40003 expired' '$T/s.out'"
has "killed client is expired after the timeout" "$T/s.out" "127.0.0.1:40004 expired"
exec 3>&- 4>&-; wait_rc "$CA"

echo "== stop =="
kill -INT "$SP"; wait_rc "$SP"
check "server exits on SIGINT (rc=$rc)" "[ $rc -eq 0 ]"
kill -INT "$TD"; wait "$TD" 2>/dev/null
check "no ICMP port unreachable: ports are held by UDP sockets" "! grep -q unreachable '$T/icmp'"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Интеграционный тест на реальной очереди. Очередь одна на всю систему (ключ из ftok),
# поэтому тест не запустится, если брокер уже работает или от него осталась очередь.
BIN=$(realpath "$(dirname "$0")/../build/pubsub")
T=$(mktemp -d)
pass=0; fail=0
rc=0
BROKER=""

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

# Если тест оборвался, не оставляем процессы и очередь в системе
cleanup() {
    exec 3>&- 2>/dev/null
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.5
    rm -rf "$T"
}
trap cleanup EXIT

run()     { timeout 10 "$BIN" "$@" >"$T/out" 2>"$T/err"; rc=$?; }
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
err_has() { grep -q -- "$2" "$T/err" && ok "$1" || bad "$1 (stderr lacks '$2')"; }
has()     { grep -q -- "$3" "$2" && ok "$1" || bad "$1 (no '$3' in $2)"; }
hasnt()   { grep -q -- "$3" "$2" && bad "$1 ('$3' found in $2)" || ok "$1"; }
same()    { printf '%b' "$3" | cmp -s - "$2" && ok "$1" || { bad "$1"; echo "--- got:"; cat "$2"; }; }

# Ждём появления строки в файле до 5 секунд
wait_for() {
    local i
    for i in $(seq 50); do grep -q -- "$2" "$1" 2>/dev/null && return 0; sleep 0.1; done
    return 1
}
# Ждём, пока строка встретится не меньше N раз
wait_count() {
    local i
    for i in $(seq 50); do
        [ "$(grep -c -- "$2" "$1" 2>/dev/null)" -ge "$3" ] && return 0; sleep 0.1
    done
    return 1
}
# Ждём завершения процесса до 5 секунд
wait_exit() {
    local i
    for i in $(seq 50); do kill -0 "$1" 2>/dev/null || return 0; sleep 0.1; done
    return 1
}
# Ждём процесс и сверяем его код возврата
exits_with() {
    if wait_exit "$2"; then
        wait "$2"; rc=$?
        [ "$rc" -eq "$3" ] && ok "$1" || bad "$1 (rc=$rc, want $3)"
    else
        bad "$1 (process still running)"
    fi
}
start_broker() {
    : >"$T/broker.out"; : >"$T/broker.err"
    "$BIN" -b >"$T/broker.out" 2>"$T/broker.err" &
    BROKER=$!
    wait_for "$T/broker.out" "started" || { bad "broker started"; cat "$T/broker.err"; return 1; }
}

echo "== invalid usage =="
run;                    want_rc "no arguments" 2
run -x;                 want_rc "unknown option" 2
run -b extra;           want_rc "-b with an argument" 2
run -p;                 want_rc "-p without topic" 2
run -p a b;             want_rc "-p with two topics" 2
run -s;                 want_rc "-s without topics" 2
run -b -p news;         want_rc "two roles at once" 2
run -p 'a,b';           want_rc "-p topic with comma" 2
err_has "diagnostic for bad topic" "invalid topic"
run -s ok 'a,b';        want_rc "-s one bad topic among good" 2

echo "== no broker =="
run -p news </dev/null; want_rc "publisher without broker" 1
err_has "diagnostic for publisher" "not running"
run -s news;            want_rc "subscriber without broker" 1
err_has "diagnostic for subscriber" "not running"

echo "== broker start =="
start_broker || exit 1
if grep -q "already running" "$T/broker.err"; then
    echo "Брокер уже запущен или осталась очередь: ipcs -q, ipcrm -q <id>"; exit 1
fi
ok "broker started"
run -b;                 want_rc "second broker refuses to start" 1
err_has "diagnostic for second broker" "already running"

echo "== delivery by topic =="
"$BIN" -s news       >"$T/s1" 2>&1 & S1=$!
"$BIN" -s news sport >"$T/s2" 2>&1 & S2=$!
"$BIN" -s sport      >"$T/s3" 2>&1 & S3=$!
wait_count "$T/broker.out" "subscribed to" 4 && ok "4 subscriptions registered" || bad "4 subscriptions registered"

printf 'hello\nsecond\n' | "$BIN" -p news;   rc=$?; want_rc "publisher: two messages" 0
printf 'goal\n'          | "$BIN" -p sport;  rc=$?; want_rc "publisher: other topic" 0
printf 'a, b  c\n\n'     | "$BIN" -p news;   rc=$?; want_rc "publisher: comma, spaces, empty line" 0
wait_for "$T/s1" "a, b  c" && wait_for "$T/s2" "a, b  c" && wait_for "$T/s3" "goal" || bad "messages delivered in time"
same "subscriber on news gets only news"     "$T/s1" 'news: hello\nnews: second\nnews: a, b  c\n'
same "subscriber on two topics gets both"    "$T/s2" 'news: hello\nnews: second\nsport: goal\nnews: a, b  c\n'
same "subscriber on sport gets only sport"   "$T/s3" 'sport: goal\n'
has "broker registered a publisher" "$T/broker.out" "message from"

echo "== unsubscribe on exit =="
kill -INT "$S1"
exits_with "subscriber exits on SIGINT" "$S1" 0
wait_for "$T/broker.out" "unsubscribed from 'news'" && ok "broker saw unsubscribe" || bad "broker saw unsubscribe"
printf 'late\n' | "$BIN" -p news
wait_for "$T/s2" "news: late" && ok "remaining subscriber still receives" || bad "remaining subscriber still receives"
hasnt "no delivery to the departed subscriber" "$T/broker.err" "dropped"

echo "== broker shutdown =="
mkfifo "$T/in"
"$BIN" -p news <"$T/in" >"$T/p.out" 2>&1 & PUB=$!
exec 3>"$T/in"
echo hi >&3
wait_for "$T/s2" "news: hi" && ok "message via long-running publisher" || bad "message via long-running publisher"

kill -INT "$BROKER"
exits_with "broker exits on SIGINT" "$BROKER" 0
exits_with "subscriber 2 stops when broker stops" "$S2" 0
exits_with "subscriber 3 stops when broker stops" "$S3" 0
exits_with "publisher stops when broker stops" "$PUB" 0
exec 3>&-
has   "broker logged the stop" "$T/broker.out" "stopping"
hasnt "queue was emptied before the timeout" "$T/broker.err" "timeout"
run -s news;            want_rc "queue is removed after the broker exits" 1
err_has "diagnostic after removal" "not running"

echo "== queue removed from outside =="
start_broker || exit 1
QID=$(sed -n 's/.*queue id \([0-9]*\).*/\1/p' "$T/broker.out")
"$BIN" -s news >"$T/s4" 2>&1 & S4=$!
wait_for "$T/broker.out" "subscribed to" || bad "subscriber registered"
ipcrm -q "$QID"
exits_with "subscriber exits when the queue disappears" "$S4" 0
exits_with "broker exits with an error when the queue disappears" "$BROKER" 1

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

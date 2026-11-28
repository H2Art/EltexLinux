#!/bin/bash
# Интеграционный тест CLI: команды через FIFO, вывод в файл.
B=$(realpath "$(dirname "$0")/../build/taxi")
T=$(mktemp -d)
pass=0; fail=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }
cleanup() { exec 3>&- 2>/dev/null; jobs -p | xargs -r kill -KILL 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT

wait_for() {    # wait_for шаблон [файл]
    local i
    for i in $(seq 100); do grep -q -- "$1" "${2:-$T/out}" 2>/dev/null && return 0; sleep 0.05; done
    return 1
}
has()  { wait_for "$2" && ok "$1" || bad "$1"; }
# gone pid...: все процессы исчезли (и не висят зомби) за 2 с
gone() { local i p; for i in $(seq 40); do for p in "$@"; do [ -e "/proc/$p" ] && { sleep 0.05; continue 2; }; done; return 0; done; return 1; }
# start: CLI в своей группе процессов (как в терминале), ввод из FIFO на дескрипторе 3
start() {
    rm -f "$T/in"; mkfifo "$T/in"
    setsid "$B" <"$T/in" >"$T/out" 2>&1 3>&- & CLI=$!
    exec 3>"$T/in"
}
create() {      # create n: создать n водителей, их pid в массиве P
    local i
    P=()
    for i in $(seq "$1"); do echo create_driver >&3; done
    for i in $(seq 100); do [ "$(grep -c created "$T/out")" -ge "$1" ] && break; sleep 0.05; done
    mapfile -t P < <(grep -o 'driver [0-9]* created' "$T/out" | awk '{print $2}')
}
cpu_ticks() { awk '{print $14 + $15}' "/proc/$1/stat"; }    # utime + stime

echo "== commands =="
start
create 2; P1=${P[0]}; P2=${P[1]}
check "two drivers created" "[ \${#P[@]} -eq 2 ]"
check "drivers are child processes of the CLI" "[ \$(ps -o ppid= -p $P1) -eq $CLI ]"
echo "send_task $P1 2" >&3;  has "driver takes a task" "driver $P1: got a task for 2 s"
echo "send_task $P1 5" >&3;  has "busy driver answers Busy <seconds>" "driver $P1: Busy 2"
echo "get_status $P2" >&3;   has "free driver is Available" "driver $P2: Available"
echo get_drivers >&3;        has "get_drivers counts drivers" "^2 drivers"
check "get_drivers reports each driver" "wait_for 'driver $P1: Busy' && [ \$(grep -c 'driver $P2: Available' '$T/out') -eq 2 ]"
echo "get_status 1" >&3;     has "unknown pid" "no driver 1"
echo "send_task $P1 0" >&3;  has "zero timer rejected" "bad command"
echo "fly" >&3
check "unknown command rejected" "wait_for 'bad command' && [ \$(grep -c 'bad command' '$T/out') -eq 2 ]"
t0=$(cpu_ticks "$P1"); sleep 1; t1=$(cpu_ticks "$P1")
check "busy driver sleeps, not spins (CPU ticks $t0 -> $t1)" "[ \$((t1 - t0)) -le 1 ]"
check "driver is in state S (sleeping in select)" "[ \"\$(ps -o stat= -p $P1 | cut -c1)\" = S ]"
has "after the timer the driver reports it is free" "driver $P1: task done, Available"
echo "get_status $P1" >&3;   has "and get_status says Available" "driver $P1: Available"

echo "== a driver dies =="
kill -KILL "$P2";            has "CLI notices the dead driver" "driver $P2 exited"
check "no zombie left" "gone $P2"
echo get_drivers >&3;        has "get_drivers shows only live drivers" "^1 drivers"

echo "== exit =="
echo exit >&3; exec 3>&-
wait "$CLI"; rc=$?
check "exit: rc 0 (rc=$rc)" "[ $rc -eq 0 ]"
check "exit: drivers finished" "gone $P1 && grep -q 'stopped, 1 drivers finished' '$T/out'"
check "no queues left in /dev/mqueue" "! ls /dev/mqueue | grep -q '^taxi_'"

echo "== Ctrl+C goes to the whole process group =="
start; create 2
kill -INT -- "-$CLI"          # как терминал: SIGINT всей группе, водители его игнорируют и ждут QUIT
wait "$CLI"; rc=$?
check "CLI exits with rc 0 (rc=$rc)" "[ $rc -eq 0 ]"
check "drivers quit by command" "gone ${P[*]} && grep -q 'stopped, 2 drivers finished' '$T/out'"
exec 3>&-

echo "== CLI killed with -9 =="
start; create 2
kill -KILL "$CLI"; wait "$CLI" 2>/dev/null
check "drivers die with the CLI (PR_SET_PDEATHSIG)" "gone ${P[*]}"
exec 3>&-
check "still no queues left" "! ls /dev/mqueue | grep -q '^taxi_'"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

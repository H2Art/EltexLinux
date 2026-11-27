#!/bin/bash
# Интеграционный тест. RAW-сокету нужен CAP_NET_RAW: вместо sudo перезапускаемся в своём
# user + network namespace (unshare -rn), там эта способность есть, а сеть — только свой lo.
S=$(realpath "$(dirname "$0")/../build/sniffer")
CHAT=$(realpath "$(dirname "$0")/../../6/build/chat")
pass=0; fail=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }
lines() { grep -c "$2" "$1"; }

if [ -z "$IN_NETNS" ]; then
    echo "== invalid usage =="
    for args in "" "-f tcp" "-f chat -c 0" "-f chat -t x" "-f chat extra"; do
        "$S" $args >/dev/null 2>&1; rc=$?
        check "usage error for '$args' (rc=$rc)" "[ $rc -eq 2 ]"
    done
    if [ "$(id -u)" -ne 0 ] && ! getcap "$S" 2>/dev/null | grep -q cap_net_raw; then
        err=$("$S" -f chat 2>&1 >/dev/null); rc=$?
        check "without CAP_NET_RAW: rc 1 and a hint (rc=$rc)" "[ $rc -eq 1 ] && grep -q 'needs CAP_NET_RAW' <<<'$err'"
    fi
    [ -x "$CHAT" ] || { echo "build task 6 first: make -C ../6"; exit 1; }
    IN_NETNS=1 PASS0=$pass FAIL0=$fail exec unshare -rn "$0"
fi
pass=$PASS0; fail=$FAIL0
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
ip link set lo up

# Клиент чата из задания 6 на loopback-broadcast: JOIN, строки, LEAVE
chat() { local name=$1; shift; printf '%s\n' "$@" | "$CHAT" "$name" 127.255.255.255 >/dev/null 2>&1; }
noise_dns()  { dig @127.0.0.1 example.com +tries=1 +time=1 >/dev/null 2>&1; }
noise_udp()  { echo "not a chat message" >/dev/udp/127.0.0.1/50999; }
start() { "$S" "$@" >"$T/out" 2>"$T/err" & SP=$!; sleep 0.3; }
finish() { wait "$SP"; rc=$?; }

echo "== errors =="
"$S" -f chat -i nosuch0 >/dev/null 2>"$T/err"; rc=$?
check "unknown interface: rc 1 (rc=$rc)" "[ $rc -eq 1 ] && grep -q 'nosuch0: No such device' '$T/err'"

echo "== chat filter =="
tcpdump -Z root -i lo -nn -e -l udp port 50999 >"$T/tcpdump" 2>/dev/null & TD=$!
sleep 0.5
start -f chat -i lo -t 3 -o "$T/file"
chat alice "hi there"
noise_dns; noise_udp
finish
kill -INT $TD; wait $TD 2>/dev/null
check "exits after -t with rc 0 (rc=$rc)" "[ $rc -eq 0 ]"
check "header says 3 segments" "grep -q 'captured 3 UDP segments, filter chat, interface lo' '$T/out'"
check "JOIN, text and LEAVE in order" \
    "[ \"\$(sed -n '3,5p' '$T/out' | grep -o '[JML] alice.*' | paste -sd'|')\" = 'J alice|M alice: hi there|L alice' ]"
check "MAC, IP and ports are printed" \
    "[ \$(lines '$T/out' '00:00:00:00:00:00 > 00:00:00:00:00:00  127.0.0.1:50999 > 127.255.255.255:50999') -eq 3 ]"
check "time grows from the start of capture" \
    "sed -n '3,5p' '$T/out' | awk '{print \$2}' | sort -c -n && awk 'NR==3 && \$2>0 && \$2<3' '$T/out' | grep -q ."
check "tcpdump saw 4 datagrams on port 50999 (3 chat + 1 other)" "[ \$(lines '$T/tcpdump' 'UDP, length') -eq 4 ]"
check "DNS and non-chat datagram to 50999 filtered out" "! grep -q 'example\|not a chat' '$T/out'"
check "same segments as tcpdump (it also sees the non-chat one)" \
    "[ \$(grep -c 'length 16\|length 25' '$T/tcpdump') -eq 3 ] && grep -q 'len 25  M alice' '$T/out'"
check "-o writes the same report to a file" "cmp -s '$T/out' '$T/file'"

echo "== dns filter =="
start -f dns -t 3
noise_dns; chat bob "hello"
finish
check "query captured on any interface" "grep -q '127.0.0.1:[0-9]* > 127.0.0.1:53  len [0-9]*  query example.com' '$T/out'"
check "chat filtered out" "! grep -q 'bob' '$T/out'"
check "each packet counted once on any (no lo duplicate)" "grep -q 'captured 1 UDP segments' '$T/out'"

echo "== real Ethernet (veth pair inside the namespace) =="
# На lo MAC нулевые. На veth настоящие, и наш broadcast для v0 — исходящий кадр
ip link add v0 address 02:00:00:00:00:01 type veth peer name v1 address 02:00:00:00:00:02
ip addr add 10.9.0.1/24 brd + dev v0; ip link set v0 up; ip link set v1 up; sleep 1
start -f chat -i v0 -t 2
"$CHAT" eve 10.9.0.255 </dev/null >/dev/null 2>&1
finish
check "outgoing frames captured with real MACs (ETH_P_ALL)" \
    "[ \$(lines '$T/out' '02:00:00:00:00:01 > ff:ff:ff:ff:ff:ff  10.9.0.1:50999 > 10.9.0.255:50999') -eq 2 ]"

echo "== stop conditions =="
start -f chat -i lo -c 2
chat carol "one" "two" "three"
finish
check "-c 2 stops after two segments (rc=$rc)" "[ $rc -eq 0 ] && grep -q 'captured 2 UDP segments' '$T/out'"
start -f chat -i lo
chat dave "before ctrl-c"
kill -INT "$SP"; finish
check "SIGINT stops capture and prints the report (rc=$rc)" "[ $rc -eq 0 ] && grep -q 'M dave: before ctrl-c' '$T/out'"

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Путь от расположения скрипта: запускать можно из любой папки
BIN=$(realpath "$(dirname "$0")/../build/filecopy")
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
pass=0; fail=0
rc=0

ok()  { echo "PASS: $1"; pass=$((pass+1)); }
bad() { echo "FAIL: $1"; fail=$((fail+1)); }

run() {
    rm -f ./*.copy
    timeout 20 "$BIN" "$@" 2>"$T/err"
    rc=$?
}
want_rc() { [ "$rc" -eq "$2" ] && ok "$1" || bad "$1 (rc=$rc, want $2)"; }
same()    { cmp -s "$1" "$1.copy" && ok "copy of '$1'" || bad "copy of '$1' differs or missing"; }
err_has() { grep -q -- "$2" "$T/err" && ok "$1" || bad "$1 (stderr lacks '$2')"; }

cd "$T" || exit 1
printf 'hello\n' > small.txt
head -c 4097 /dev/urandom > odd.bin
head -c 4096 /dev/urandom > exact.bin
head -c 10485760 /dev/urandom > big.bin
: > empty.txt
echo data > "with space.txt"
mkdir dir

echo "== invalid usage =="
run;                    want_rc "no arguments" 2
run -p;                 want_rc "-p without value" 2
run -x small.txt;       want_rc "unknown option" 2
run -p ff;              want_rc "-p without files" 2

echo "== unnamed pipe =="
run small.txt;                  want_rc "single file" 0; same small.txt
run small.txt odd.bin exact.bin empty.txt "with space.txt"
want_rc "several files" 0
for f in small.txt odd.bin exact.bin empty.txt "with space.txt"; do same "$f"; done
run big.bin;                    want_rc "10 MB file" 0; same big.bin
run small.txt small.txt;        want_rc "same file twice" 0; same small.txt

echo "== errors =="
run missing.txt;                want_rc "only missing file" 1
err_has "diagnostic for missing" "missing.txt"
run missing.txt small.txt odd.bin; want_rc "missing among good" 1
err_has "diagnostic for missing (mixed)" "missing.txt"
same small.txt; same odd.bin
run dir small.txt;              want_rc "directory argument" 1
err_has "diagnostic for directory" "dir"
same small.txt

echo "== named pipe (-p) =="
run -p ff small.txt;            want_rc "-p single file" 0; same small.txt
run -p ff small.txt odd.bin big.bin missing.txt empty.txt
want_rc "-p several files + missing" 1
same small.txt; same odd.bin; same big.bin; same empty.txt
[ ! -e ff.p2c ] && [ ! -e ff.c2p ] && ok "fifos removed" || bad "fifos left behind"
mkfifo ff.p2c
run -p ff small.txt;            want_rc "-p fifo already exists" 1
rm -f ff.p2c ff.c2p

echo
echo "passed: $pass, failed: $fail"
[ "$fail" -eq 0 ]
#!/bin/bash
# Наглядный сеанс: брокер, три подписчика и издатели в одном терминале.
# У каждого процесса свой цвет и метка. Запуск: ./demo.sh
#   DEMO_DELAY=2 ./demo.sh   пауза между шагами в секундах (по умолчанию 1.5)
#   DEMO_STEP=1  ./demo.sh   переход к следующему шагу по Enter
BIN=$(realpath ./build/pubsub)
DELAY=${DEMO_DELAY:-1.5}
T=$(mktemp -d)

if [ -t 1 ]; then
    BOLD=$'\033[1m'; DIM=$'\033[2m'; RESET=$'\033[0m'
    C_BROKER=$'\033[33m'; C_A=$'\033[36m'; C_B=$'\033[32m'; C_C=$'\033[35m'; C_PUB=$'\033[34m'
else
    BOLD=""; DIM=""; RESET=""; C_BROKER=""; C_A=""; C_B=""; C_C=""; C_PUB=""
fi

say()   { printf '\n%s▶ %s%s\n' "$BOLD" "$*" "$RESET"; }
note()  { printf '%s  %s%s\n' "$DIM" "$*" "$RESET"; }
cmd()   { printf '%s  $ %s%s\n' "$DIM" "$*" "$RESET"; }
pause() {
    if [ -n "$DEMO_STEP" ]; then read -r -p "  [Enter] " _ </dev/tty
    else sleep "$DELAY"; fi
}

# Запускает процесс в фоне и подписывает каждую его строку цветной меткой.
# Через process substitution $! остаётся pid самой программы, а не фильтра sed,
# поэтому ей можно послать сигнал.
spawn() {
    local color=$1 label=$2 var=$3
    shift 3
    "$@" <&0 > >(sed -u "s/^/${color}[${label}]${RESET} /") 2>&1 &
    printf -v "$var" '%s' "$!"
}

reap() {
    wait "$2"
    printf '%s  %s завершился, код %s%s\n' "$DIM" "$1" "$?" "$RESET"
}

cleanup() {
    exec 3>&- 2>/dev/null
    jobs -p | xargs -r kill -INT 2>/dev/null
    sleep 0.3
    rm -rf "$T"
}
trap cleanup EXIT

[ -x "$BIN" ] || { echo "Нет $BIN, сначала выполни make"; exit 1; }
# Если очередь уже есть, издатель с пустым вводом откроет её и сразу выйдет с кодом 0
if "$BIN" -p probe </dev/null 2>/dev/null; then
    echo "Очередь уже существует (запущен брокер или остался мусор). Смотри ipcs -q, удаляй ipcrm -q <id>"
    exit 1
fi

echo "${BOLD}Демонстрация: Издатель - Брокер - Подписчик (очереди System V)${RESET}"
echo "Метки: ${C_BROKER}[брокер]${RESET} ${C_A}[подписчик A]${RESET} ${C_B}[подписчик B]${RESET} ${C_C}[подписчик C]${RESET} ${C_PUB}[издатель]${RESET}"

say "Шаг 1. Очереди пока нет"
cmd "ipcs -q"
ipcs -q
pause

say "Шаг 2. Запускаем брокера: он создаёт очередь"
cmd "pubsub -b"
spawn "$C_BROKER" "брокер" BROKER "$BIN" -b
sleep 0.5
cmd "ipcs -q"
ipcs -q
pause

say "Шаг 3. Второй брокер отказывается стартовать (очередь уже есть)"
cmd "pubsub -b"
"$BIN" -b
note "код возврата: $?"
pause

say "Шаг 4. Подписчики: A на news, B на news и sport, C на sport"
cmd "pubsub -s news"
spawn "$C_A" "подписчик A" SA "$BIN" -s news
cmd "pubsub -s news sport"
spawn "$C_B" "подписчик B" SB "$BIN" -s news sport
cmd "pubsub -s sport"
spawn "$C_C" "подписчик C" SC "$BIN" -s sport
pause

say "Шаг 5. Издатель пишет в тему news: получат A и B, но не C"
cmd "printf 'Вышла новая версия\\nВторая новость\\n' | pubsub -p news"
printf 'Вышла новая версия\nВторая новость\n' | "$BIN" -p news
pause

say "Шаг 6. Издатель пишет в тему sport: получат B и C, но не A"
cmd "echo 'Гол на последней минуте' | pubsub -p sport"
echo 'Гол на последней минуте' | "$BIN" -p sport
pause

say "Шаг 7. Тема без подписчиков: брокер сообщение принял, но отдавать его некому"
cmd "echo 'Завтра дождь' | pubsub -p weather"
echo 'Завтра дождь' | "$BIN" -p weather
pause

say "Шаг 8. Подписчик A уходит (Ctrl+C): он сообщает брокеру об отписке"
cmd "kill -INT <pid подписчика A>"
kill -INT "$SA"
reap "подписчик A" "$SA"
pause
note "Снова пишем в news: теперь получит только B"
cmd "echo 'Новость после ухода A' | pubsub -p news"
echo 'Новость после ухода A' | "$BIN" -p news
pause

say "Шаг 9. Издатель, который держит ввод открытым (пишем ему вручную через канал)"
mkfifo "$T/in"
exec 3<>"$T/in"
cmd "pubsub -p news < канал"
spawn "$C_PUB" "издатель" PUB "$BIN" -p news <&3
cmd "echo 'Сообщение от долгоживущего издателя' > канал"
echo 'Сообщение от долгоживущего издателя' >&3
pause

say "Шаг 10. Останавливаем брокера (Ctrl+C): он будит всех сигналом, ждёт и удаляет очередь"
cmd "kill -INT <pid брокера>"
kill -INT "$BROKER"
reap "брокер" "$BROKER"
reap "подписчик B" "$SB"
reap "подписчик C" "$SC"
reap "издатель" "$PUB"
exec 3>&-
pause

say "Шаг 11. Очереди больше нет, новые клиенты брокера не находят"
cmd "ipcs -q"
ipcs -q
cmd "pubsub -s news"
"$BIN" -s news
note "код возврата: $?"
sleep 0.3

echo
echo "${BOLD}Готово.${RESET}"

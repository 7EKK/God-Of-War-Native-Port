#!/usr/bin/env bash
# Usage (WSL, Citron started with use_gdbstub=true, see docs/switch.md "Debugging in Citron with gdb"):
#   tools/switch_gdbprof.sh <out file> <samples> <seconds between samples>; then python tools/switch_gdbprof.py <out file>
# samples every thread's current frame from Citron's gdb stub: interrupt, info threads, bt of the first frames, continue
OUT=${1:-/root/gdbprof.out}; N=${2:-400}; GAP=${3:-3}
rm -f /tmp/gfifo; mkfifo /tmp/gfifo
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-gdb -q -nx < /tmp/gfifo > "$OUT" 2>&1 &
G=$!
exec 3>/tmp/gfifo
echo "set pagination off" >&3; echo "set confirm off" >&3; echo "set print frame-arguments none" >&3
echo "symbol-file -o 0x80000000 /root/gow-switch-build/ps2xRuntime/ps2EntryRunner.elf" >&3
HOST=$(ip route | awk '/default/ {print $3; exit}')   # the Windows host as WSL sees it
echo "target remote $HOST:6543" >&3
echo "continue" >&3
sleep 90
for i in $(seq 1 $N); do
  kill -INT $G; sleep 1
  echo "echo ===SAMPLE $i\n" >&3
  echo "thread apply all bt 6" >&3
  echo "continue" >&3
  sleep $GAP
  kill -0 $G 2>/dev/null || break
done
kill -INT $G; echo "kill" >&3; echo "quit" >&3; exec 3>&-
wait $G

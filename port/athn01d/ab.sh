#!/bin/sh
# A/B run: ab.sh <exe dir> <label>. One ATHN01D level run (45 s stay) with the GS worker trace from 50 s;
# prints fps min/mean (report) and the per-frame worker job / EE gap (port/vu1perf/gap.py).
ROOT="/c/Users/lovas/Claude/God Of War"
cd "$ROOT" || exit 1
OUT="port/athn01d/ab/$2"
PS2X_GS_WORKER_TRACE=50 python -I tools/levels.py --zones ATHN01D --stay 45 --silent ${IDLE:+--idle} --exe-dir "$1" --out "$OUT" > /dev/null 2>&1
R=$(ls -d "$OUT"/*/ | tail -1)
FPS=$(grep "| ATHN01D" "$R/report.md" | awk -F'|' '{print $5}')
GAP=$(python -I port/vu1perf/gap.py "$R/batch0.log")
echo "$2 fps$FPS $GAP"

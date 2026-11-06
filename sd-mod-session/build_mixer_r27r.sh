#!/bin/bash
# r27r + Mixer: регрессия r27r, сборка с Mixer и тесты Mixer в WSL.
# Запуск: из sd-mod-session рабочей копии ветки feature/mixer-song-stems-r27r.
# Лог целиком копируется в Windows-папку проекта (tmp/), чтобы его можно было
# прочитать без WSL. Карта памяти, устройство и релиз не затрагиваются.
set -u
cd "$(dirname "$0")"
WIN="/mnt/c/Users/Алексей/Desktop/akai mpc sample"
R27_BUILD="${R27_BUILD:-/mnt/c/Users/Алексей/.codex/worktrees/am-mod-seq-colour-r27q/sd-mod-session/build}"
ELF="$WIN/extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
LOG="build/mixer-r27r-run.log"
mkdir -p build

# Входные файлы сборки r27r (игнорируются git, поэтому берём из worktree r27r).
if [ ! -f build/workspace-fix/bootstrap ]; then
  cp -rn "$R27_BUILD"/. build/ || { echo "Нет $R27_BUILD"; exit 2; }
fi
[ -e ../extracted_v1.3.0 ] || ln -s "$WIN/extracted_v1.3.0" ../extracted_v1.3.0
export MPC_RUNTIME_ROOT="$(cd .. && pwd)"

: > "$LOG"
step() {  # step NAME CMD...
  local name="$1"; shift
  echo "=== $name" | tee -a "$LOG"
  if "$@" >>"$LOG" 2>&1; then echo "PASS $name" | tee -a "$LOG"
  else echo "FAIL $name (exit $?)" | tee -a "$LOG"; fi
}
{ echo "branch: $(git branch --show-current) $(git rev-parse --short HEAD)"; date; } >> "$LOG"

step r27r-build        python3 build_chords_candidate.py
step r27r-seq-controls python3 verify_seq_controls.py "$ELF"
step r27r-pad-controls python3 verify_pad_colour_controls.py
step r27r-pad-rgb      python3 verify_pad_colour_rgb.py
step mixer-r27r-build  python3 build_mixer_r27r.py
for t in model meters buses archive control input input_native fx rgb session sources native route_native step1; do
  step "mixer-$t" python3 "verify_mixer_$t.py"
done

echo "=== SUMMARY" | tee -a "$LOG"
grep -E "^(PASS|FAIL) " "$LOG" | tee "build/mixer-r27r-summary.txt"
mkdir -p "$WIN/tmp"
cp "$LOG" "$WIN/tmp/mixer-r27r-run.log"
cp build/mixer-r27r-summary.txt "$WIN/tmp/mixer-r27r-summary.txt"
echo "Лог: akai mpc sample/tmp/mixer-r27r-run.log"

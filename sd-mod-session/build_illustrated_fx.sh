#!/bin/bash
# Сборка кандидата r28e-fx1 (ветка feature/illustrated-fx) в WSL. SD-карта и устройство не затрагиваются.
# Запуск: bash "/mnt/c/Users/Алексей/Desktop/am-mod-illustrated-fx/sd-mod-session/build_illustrated_fx.sh"
set -eu
WIN="/mnt/c/Users/Алексей/Desktop/akai mpc sample"
SRC="/mnt/c/Users/Алексей/Desktop/am-mod-illustrated-fx"
SEED="/home/user/am-mod-stems-merge/sd-mod-session/build"
BASE_ZIP="$WIN/releases/r28d/am-mod-r28d.zip"
VER="r28e-fx1"
W=/home/user/am-mod-illustrated-fx
mkdir -p "$W"
# Свежие исходники ветки; build/ и .git не трогаем.
( cd "$SRC" && tar --exclude=.git --exclude=sd-mod-session/build -cf - . ) | ( cd "$W" && tar -xf - )
[ -e "$W/extracted_v1.3.0" ] || ln -s "$WIN/extracted_v1.3.0" "$W/extracted_v1.3.0"
cd "$W/sd-mod-session"
export MPC_RUNTIME_ROOT="$W"
mkdir -p build
[ -f build/workspace-fix/bootstrap ] || cp -rn "$SEED"/. build/ || { echo "Нет $SEED"; exit 2; }
sed -i -E "s/AM_MOD_VERSION \"[^\"]+\"/AM_MOD_VERSION \"$VER\"/" src/mod_build.h
sed -i -E "s/^VERSION=\"[^\"]+\"/VERSION=\"$VER\"/" package_routing_candidate.py
LOG=build/$VER-run.log; : > "$LOG"
# 1. Проверки на хосте (ASan/UBSan): свои эффекты, вид микшера.
F="-std=c11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -fsanitize=address,undefined -fno-sanitize-recover=all"
SRCS="src/mixer_view.c src/fx_catalog.c src/mixer_buses.c src/mixer_model.c src/mixer_meters.c src/fx_chain.c src/fx_eq.c src/fx_own.c src/compressor_dsp.c"
fail=0
gcc $F check_fx_own.c src/fx_own.c src/compressor_dsp.c -lm -o build/check_fx_own >>"$LOG" 2>&1 && build/check_fx_own >>"$LOG" 2>&1 && echo "PASS check_fx_own" || { echo "FAIL check_fx_own"; fail=1; }
gcc $F check_mixer_view.c $SRCS -lm -pthread -o build/check_mixer_view_host build >>"$LOG" 2>&1 && build/check_mixer_view_host >>"$LOG" 2>&1 && echo "PASS check_mixer_view" || { echo "FAIL check_mixer_view"; fail=1; }
# 2. Сборка библиотеки и проверки r28e.
python3 build_mixer_r27r.py 2>&1 | tee -a "$LOG"
for v in verify_pad_lighting.py verify_mixer_control.py verify_mixer_step1.py verify_mixer_rgb.py \
         verify_sample_relation_scope.py verify_sample_playing.py verify_mixer_native.py verify_pad_colour_rgb.py; do
  if python3 "$v" >>"$LOG" 2>&1; then echo "PASS $v"; else echo "FAIL $v"; fail=1; fi
done
if [ "$fail" != 0 ]; then mkdir -p "$WIN/releases/$VER"; cp "$LOG" "$WIN/releases/$VER/run.log"; echo "Проверки не прошли: $WIN/releases/$VER/run.log"; exit 1; fi
# 3. Упаковка поверх r28d.
rm -rf "build/$VER"
python3 package_routing_candidate.py "$BASE_ZIP"
mkdir -p "$WIN/releases/$VER"
cp -r "build/$VER/." "$WIN/releases/$VER/"
echo "Готово: $WIN/releases/$VER/am-mod-$VER.zip"

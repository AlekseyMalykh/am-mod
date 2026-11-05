#!/bin/sh
# Build, test and package r27q-fix2 in WSL. Run from sd-mod-session.
set -e
ELF="/mnt/c/Users/Алексей/Desktop/akai mpc sample/extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
BASE="/mnt/c/Users/Алексей/Desktop/akai mpc sample/am-mod-r27q.zip"
python3 build_chords_candidate.py
python3 verify_seq_controls.py "$ELF"
python3 verify_pad_colour_controls.py
python3 verify_pad_colour_rgb.py
python3 package_bugfix_candidate.py "$BASE" build/r27q-fix2/am-mod-r27q-fix2.zip
echo "BUILD_R27Q_FIX2_OK"

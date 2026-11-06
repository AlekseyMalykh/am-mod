#!/bin/sh
# Build, test and package Am MOD r27r in WSL. Run from sd-mod-session.
set -e
ELF="/mnt/c/Users/Алексей/Desktop/akai mpc sample/extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample"
python3 build_chords_candidate.py
python3 verify_seq_controls.py "$ELF"
python3 verify_pad_colour_controls.py
python3 verify_pad_colour_rgb.py
python3 verify_early_drm.py
python3 package_r27r.py
echo "BUILD_R27R_OK"

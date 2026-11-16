#!/bin/sh
# Run in the prepared Linux/WSL build workspace with pinned firmware and
# frozen bootstrap inputs. Produces a ZIP only; never installs to a card.
set -eu
[ "$#" = 1 ] || { echo 'usage: build_usb_candidate.sh /path/to/am-mod-r30a.zip' >&2; exit 2; }
base_zip=$(realpath "$1")
export AM_USB_BASE_ZIP="$base_zip"
cd "$(dirname "$0")"
python3 verify_audio_clicks.py
python3 verify_fx_layout.py
python3 verify_mixer_control.py
python3 verify_mixer_step1.py
python3 verify_mixer_buses.py
python3 verify_modern_fx.py
python3 verify_compressor.py
python3 verify_pad_colour_rgb.py
python3 verify_sample_overlay.py
python3 verify_input_config_ui.py
python3 verify_rear_mix_input_fx.py
python3 verify_usb.py
python3 verify_usb_drive.py
python3 verify_screen_mirror.py
python3 verify_usb_native.py
python3 verify_pad_lighting.py
python3 verify_mixer_native.py
python3 build_usb_names.py
python3 verify_usb_names_vm.py
python3 build_mixer_r27r.py
python3 package_usb_candidate.py "$base_zip"

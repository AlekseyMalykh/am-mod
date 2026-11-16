# Building and verifying r30b

This repository contains mod code, tests and documentation, **not** the stock firmware/rootfs or factory content. The current release build still depends on frozen outputs from earlier verified builds. A clean clone alone is not enough; missing private inputs must fail rather than silently substitute another firmware.

## Inputs

- Linux/WSL, Python 3, GCC/binutils for AArch64, host GCC/G++, QEMU user/system tools.
- Your lawful firmware **1.3.0.12**, extracted under `extracted_v1.3.0/`; `usr/bin/MPC Sample` SHA-256: `b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`.
- A compatible local rootfs for QEMU. Tests default to `/home/user/mpc-rootfs-user-20260808`; individual scripts expose `MPC_QEMU_SYSROOT` or pin this path. Read each verifier before changing paths.
- Verified r30a base ZIP, SHA-256 `1e3c26a15f0a082a0847ab4dcc4ed29bbed11d81167ea7fc1cafe17e9868131d`, plus frozen bootstrap assets expected by `build_mixer_r27r.py`.
- Named USB module: official inMusic `az0x-open-source` branch `mpc-sample/1.3.0`, archive `aarch64-poky-linux/linux-imb-6.12.60+git-r0/linux-imb-6.12.60+git-r0-patched.tar.xz`, SHA-256 `9d6999d9f69f5afd396a634facadc9219dcb85407264273d912355dd6db38f9b`.
- Exact **main** firmware kernel/config: `6.12.60-imb-2025-12-03-rt13`. The recovery kernel is different and must not be substituted. Module source/config, transformation script and compile/VM proofs ship under `SOURCE/usb-audio-names/` in the release ZIP.

## Main entry points

Run from `sd-mod-session/` in the prepared workspace:

```bash
python3 verify_audio_clicks.py
python3 verify_fx_layout.py
python3 verify_mixer_control.py
python3 verify_mixer_step1.py
python3 verify_mixer_buses.py
python3 verify_input_config_ui.py
python3 verify_rear_mix_input_fx.py
python3 verify_compressor.py
# USB ownership/native tap/full build/packaging, using the verified r30a ZIP:
sh build_usb_candidate.sh /path/to/am-mod-r30a.zip
```

`verify_compressor.py` also reads the historical DSP baseline from Git commit `152e07c`. A WSL source mirror without Git metadata needs access to the real repository's Git database for that baseline; do not replace the baseline with current code.

`package_usb_candidate.py` keeps its development-era filename but produces the public **r30b** ZIP. It checks proof source hashes and the pinned base package before packaging. It never writes a physical card. The installer is a separate step.

Check results under `sd-mod-session/build/`. Do not commit firmware, rootfs, extracted app code, device logs/identifiers, private projects or build outputs. Some historical verification scripts predate current UI mappings; a passing focused suite is not a claim that every old script passes.

## Media without firmware

See [FX gallery](FX_GALLERY.md). `build_fx_gifs.py` compiles the production painter, uses generated test signals and records source/GIF hashes. Pillow and DejaVu Sans are needed only for this rendering step.

## Limits

Native/QEMU fixtures execute selected real firmware functions with isolated service/GUI/audio adapters. They do not prove physical timing, USB host compatibility or every project transition. Keep hardware observations separate from automatic checks; see [release notes](RELEASE_NOTES-r30b.md).

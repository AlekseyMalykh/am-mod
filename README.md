# AM Mod

**Unofficial SD card mod for the Akai MPC Sample**

**English** · [Русский](README.ru.md)

AM Mod is a community mod for the Akai MPC Sample (firmware **1.3.0.12**) that starts from the SD card.
It adds a step sequencer on the pads, a chord mode, typing names with the pads, scale
lighting, search across folders, batch copy, a per-pad EQ, resampling and more, on top of
the stock app. Nothing is flashed: the mod runs a patched copy of the stock app in RAM, and
**Project → Mod Off** brings the stock app back.

<p align="center"><img src="docs/media/chords.gif" alt="Chords mode: pads play chords in the project key" width="760"></p>
<p align="center"><sub>Recorded in the <a href="https://github.com/orrpunk-art/virtual-mpc-sample">Virtual MPC Sample</a> emulator running the stock firmware with the mod; the pads on the right show the colours the app sends.</sub></p>

> **Unofficial project.** Not affiliated with, endorsed by or supported by inMusic Brands
> or Akai Professional. **No firmware and no Akai code are in this repository or in the
> release packages**: the mod patches the copy of the app already on your MPC Sample.
> Use at your own risk. See [NOTICE](NOTICE.md).

## Features

Status as of 31 October 2026 (build r24x). **Confirmed** means checked on a real MPC Sample;
**trial** means it passes the ARM/QEMU checks and the emulator but waits for a hardware run.

| Feature | Where | Status |
|---|---|---|
| **Readable K1–K3 panel**: parameter names and values always visible, amber focus, `S` marks for Shift functions | all standard screens | confirmed |
| **Type names with the pads** (phone keypad: ABC on pad 14, DEF on 15…, delete on 16) | Save Project / Save Sample, Search | confirmed |
| **Orig Note** of a sample, project **Key / Scale** | Sample → Tune → Shift+K1; Project → Key/Scale | confirmed |
| **Scale lighting** for 16 Levels Tune: Piano / Tonic / Scale colours, Transpose16 | Sample → Tune/Play/**Lights** | partly confirmed |
| **Steps**: 16-step sequencer on the physical pads, grid 1/4…1/64 and triplets, velocity, mute steps, copy/paste | Shift+SEQ → B2 Steps | confirmed |
| **Events editor** with Chop / 16 Levels choice and audition, event move on the grid | Step Edit / Steps | confirmed |
| **Poly**: chords recorded and edited as separate notes | Seq | confirmed |
| **Search** samples and projects in Folder / Subfolders / Library, preview, load to a pad | Sample Select / Open Project → Shift+B3 | confirmed (basic) |
| **Batch Copy** of up to 16 sounds with automatic or manual targets | Sample → Shift+Erase → B2 | confirmed |
| **Resample** a pad (with Pad Link and Bypass) or a sequence to another pad | Shift+Pad 11 | confirmed |
| **Power key** asks, saves a separate copy of the project to SD, powers off | power key | confirmed |
| **Per-pad EQ**: one band, 7 types, ±18 dB, live spectrum behind the curve | Sample → B3 Filter → Filter Env → **EQ** | confirmed |
| **Stereo → Mono** (left or right channel to a new pad) | Sample → Shift+B3 | trial |
| **Chords**: 8 scale degrees, 6/7/maj7/9, Maj/Min/Dim/Aug/Sus4, octave, inversion, latch, arpeggiator, chord repeat with Note Repeat; recorded as normal Tune events | 16 Levels Tune → Chop | trial (r24v) |
| **Sequence pads**: 13 colours (Stock by default); the playing and the queued sequence blink with the tempo | Seq → Shift+K1 | trial (r24v) |
| **Batch Extract** chops to separate samples | Chop → Shift+B2 | confirmed |

Every feature with its controls and status: [docs/FEATURES.md](docs/FEATURES.md). Build history: [CHANGELOG.md](CHANGELOG.md).

### In action

<table>
<tr>
<td width="50%"><b>Steps</b><br><img src="docs/media/steps.gif" alt="Steps" width="100%"></td>
<td width="50%"><b>Typing names with the pads</b><br><img src="docs/media/pad-typing.gif" alt="Typing names with the pads" width="100%"></td>
</tr>
<tr>
<td width="50%"><b>Scale lighting</b><br><img src="docs/media/scale-lights.gif" alt="Scale lighting" width="100%"></td>
<td width="50%"><b>Search</b><br><img src="docs/media/search.gif" alt="Search" width="100%"></td>
</tr>
<tr>
<td width="50%"><b>Batch Copy</b><br><img src="docs/media/batch-copy.gif" alt="Batch Copy" width="100%"></td>
<td width="50%"><b>Pad EQ</b><br><img src="docs/media/pad-eq.gif" alt="Pad EQ" width="100%"></td>
</tr>
<tr>
<td width="50%"><b>Resample a pad</b><br><img src="docs/media/resample.gif" alt="Resample a pad" width="100%"></td>
<td width="50%"></td>
</tr>
</table>

## Install

You need an MPC Sample on firmware **1.3.0.12** (the mod checks the app's SHA-256 and
refuses to start on any other version) and an SD card formatted by the MPC.

**Installer (Windows):** download `AM-Mod-Installer.exe` and the
`am-mod-*.zip` package from [Releases](../../releases), run the installer, pick the zip
(or press *Latest release*) and the SD card, press **Install**.

**Installer (macOS / Linux / Windows with Python 3.8+):**

```bash
python3 installer/mpc_mod_installer.py                 # window
python3 installer/mpc_mod_installer.py cards           # list cards
python3 installer/mpc_mod_installer.py install am-mod-r24x.zip --card E:
python3 installer/mpc_mod_installer.py install --latest --card /Volumes/MPC
python3 installer/mpc_mod_installer.py remove --card E:
```

The installer writes only `autorun/` and `MPC_MOD_VERSION.json` on the card. It checks the
package, backs up the previous `autorun/` to `~/AM-Mod-Backups/`, writes the files
that start the mod last, and verifies everything it wrote. It never formats the card and
never touches `MPC-Sample/` (your projects and samples).

**By hand:** unzip the package and copy its `autorun` folder to the root of the card.

## Use

1. Insert the card and power on the MPC, or insert it into a running MPC. The mod starts
   by itself (since r22 there is no extra question) and takes a few seconds. If a start
   ever hangs at 0 %, power on without the card, wait for the normal screen, then insert it.

   > **The screen goes dark when the mod starts.** The stock app closes first, so the
   > screen and the pads go out for a few seconds before the mod appears. This is normal:
   > do not power off and do not pull the card. The same happens on Mod Off, when the stock
   > app comes back.

2. The mod checks the stock app and its own files before it starts; on any mismatch the
   stock app keeps running.
3. Work as usual. Save projects and samples to **External** (the SD card): in the mod,
   Internal storage is temporary and is lost when the mod stops.
4. To leave: **Project** (Shift+Pad 16) → **Mod Off** → confirm. The mod saves a copy of
   the project to `MPC-Sample/Projects/Mod-Exit-…` and the stock app returns. Then eject
   the card as usual; in Windows use *Eject*.

The power key also works in the mod: it asks, saves a project copy to the card and powers
off. Do not pull the card while the mod is starting, saving or shutting down.

## Try it without the device

[**Virtual MPC Sample**](https://github.com/orrpunk-art/virtual-mpc-sample) runs the stock
firmware in QEMU with the whole device in your browser, and runs SD autorun packages the
way the MPC does. The GIFs above were recorded there:

```bash
python3 lab.py autorun am-mod-r24x.zip --no-reboot --yes
```

Emulator runs are useful for development, but they are not a hardware test: timing, audio
latency, battery and USB audio differ from the device.

## Build from source

Requirements: Linux or WSL, Python 3, AArch64 GCC/binutils and your **own** local copy of
the stock firmware 1.3.0.12; the expected SHA-256 of the app is
`b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`. The firmware image,
updater, extracted partitions and rootfs are not part of this repository; paths to the
rootfs in older checks (`/home/user/mpc-rootfs-user-20260808`) need adjusting.

Order: get the local inputs with the expected hashes → build `sd-clone-trial` (the loader)
→ build the value patches in `sd-k-value-peek` → build the current `sd-mod-session`
revision. For r24x: `sd-mod-session/build_chords_candidate.py`, `verify_chords.py`,
`verify_pad_eq.py`, then `package_update.py` over the previous package. The ARM/QEMU checks (`verify_*.py`, `check_*.c`) use clearly marked
adapters and do not replace a hardware run.

| Folder | Contents |
|---|---|
| `sd-mod-session/` | the current mod: sources (`src/`), build and package scripts, checks, feature docs |
| `sd-clone-trial/` | the loader: runs a verified copy of the stock app in RAM and restores the stock app |
| `sd-k-value-peek/` | the first display patches (K1–K3 values) |
| `installer/` | the SD card installer and its tests |
| `virtual-mpc/` | an early browser model of the UI (not an emulator) |
| `research/`, `docs/` | reverse-engineering notes and scripts, history of the stages |

Most technical documents are in Russian.

## Legal

- Akai Professional and MPC are trademarks of inMusic Brands, Inc., used here only to name
  the device the mod runs on.
- No firmware, Akai code, factory content or Akai artwork is distributed. The mod copies
  the app that is on your device and patches that copy in RAM; it never writes the
  firmware or the boot loader.
- Release packages include `xdg-dbus-proxy` (LGPL-2.1-or-later) with its source.
- Details: [NOTICE.md](NOTICE.md).

## License

[MIT](LICENSE). Contributions: [CONTRIBUTING.md](CONTRIBUTING.md).

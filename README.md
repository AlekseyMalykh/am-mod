# AM Mod

**Unofficial SD card mod for the Akai MPC Sample**

**English** · [Русский](README.ru.md)

[Feedback and help](docs/FEEDBACK.md) · [Known issues](docs/KNOWN_ISSUES.md) · [Discussions](https://github.com/AlekseyMalykh/am-mod/discussions)

[**Release r26s: download the package and installer**](https://github.com/AlekseyMalykh/am-mod/releases/tag/r26s)

OptoCat/Warm, the K fix, workflow pad colours, Seq pulses, Sound cursor and EQ
side indicators. **FAT32 is experimental**; exFAT is supported.
[Release notes](docs/RELEASE_NOTES-r26s.md) · [Remaining reports](docs/KNOWN_ISSUES.md).
The author confirmed the K fix on an MPC; automated coverage and remaining
physical checks are stated in the release notes.

AM Mod is a community mod for the Akai MPC Sample (firmware **1.3.0.12**) that starts from the SD card.
It adds a step sequencer on the pads, a chord mode, typing names with the pads, scale
lighting, search across folders, batch copy, a per-pad EQ, resampling and more, on top of
the stock app. Nothing is flashed: the mod runs a patched copy of the stock app in RAM.
Candidate r27e saves a recovery copy on normal shutdown; public r26s still includes
**Project → Mod Off**.

<p align="center"><img src="docs/media/chords.gif" alt="Chords mode: pads play chords in the project key" width="760"></p>
<p align="center"><sub>Recorded in the <a href="https://github.com/orrpunk-art/virtual-mpc-sample">Virtual MPC Sample</a> emulator running the stock firmware with the mod; the pads on the right show the colours the app sends.</sub></p>

> **Unofficial project.** Not affiliated with, endorsed by or supported by inMusic Brands
> or Akai Professional. **No firmware and no Akai code are in this repository or in the
> release packages**: the mod patches the copy of the app already on your MPC Sample.
> Use at your own risk. See [NOTICE](NOTICE.md).

## r27e prerelease

r27e is a test prerelease; the stable download above remains r26s. **Back up samples and projects before updating.**

The new package combines all changes since r26s:

- **Mod Settings:** five circular B2/B3 tabs, Standard/AM Mod, waveform/spectrum
  tint, service/user palette swaps, confirmed recommended reset, About.
- **Readable pad selection:** configurable white cursors in Sample, Chop, browser,
  Copy/Batch/Extract/Mono/Render and the Sound picker, including empty selected
  Sample pads and the actual current browser load target. Empty unselected
  destinations stay off; selected free/occupied targets remain green/red.
- **Defaults:** AM Mod + Normal (400ms white/400ms base). Steady/Slow/Normal/Fast
  are configurable. Saved card/project preferences take priority. One cursor
  marks editing selection while several samples can still sound together.
- **Standard** retains native pad lighting plus 16 Levels Piano/Scale layouts
  and Transpose16; user Pad/Seq Colour overrides are disabled without data loss.
- **Pad Bank + turn K** resets supported Sample/EQ/OptoCat/Lights/Chords/Steps/
  Key/Scale controls; hold Pad Bank, then Shift for the Shift control. Bank
  selection with pads1–8 is preserved. Native Duck/other FX are not reset.
- **Projects:** display/OptoCat settings in XPJ, recovered identity and first-Save
  confirmation, missing Temp-Project autosave fix, one complete startup SD Load.
- **Startup/shutdown:** small version/repository on the Am MOD splash and About;
  normal-power recovery saving; Project Mod Off removed. Preparation time varies.
- **Steps:** exclusive WRITE on SEQ RECORD and TOGGLE on SAMPLE RECORD.
- **Corrections:** browser ERASE, Note Repeat priority/Type redraw, spectrum tint
  with native filter/envelope curves and EQ side meters preserved.
- **Included:** the confirmed native K fix, OptoCat/Warm (Gain−6…+6dB, Shift
  Attack/Release/Knee, shared Bypass), all earlier musical/search/editing tools.

**exFAT supported; FAT32 experimental.** Installer1.0.2 is unchanged: choose
the local r27e ZIP for a prerelease; Latest continues to select stable releases.
ARM/UBSan and targeted native VM checks passed. Exact-package full VM boot and
final cursor/hardware scenarios are not claimed complete.

[Complete changes since r26s](docs/RELEASE_NOTES-r27e.md) ·
[All features and controls](docs/FEATURES.md) · [Mod Settings](docs/MOD_SETTINGS.md) ·
[Remaining reports/requests](docs/KNOWN_ISSUES.md) · [Readiness](docs/RELEASE_READINESS-r27e.md).

Synth, stem export, mixer/effect routing and an independent RGB editor remain
requests, not features in this package. Thanks to Leusha, mxt1iga and all testers.

Ideas C01/C02: live held-chord/arp changes and force-based expression are [future requests](docs/IDEAS.md), not implemented in r27e. New-hit velocity already passes through; continuous pressure still needs investigation.
Plan M01: audit MIDI In/Out, channels/clock, external Chords/arp operation, velocity and Note On/Off. Details are in the same roadmap; verification is pending.

## Features

Local r27e candidate documentation, 3 November 2026; the public ZIP is r26s. **Confirmed** means checked on a real MPC Sample;
**trial** means the specified automated checks passed; hardware coverage is pending as noted.

| Feature | Where | Status |
|---|---|---|
| **Readable K1–K3 panel**: parameter names and values always visible, amber focus, `S` marks for Shift functions | all standard screens | confirmed |
| **Type names with the pads** (phone keypad: ABC on pad 14, DEF on 15…, delete on 16) | Save Project / Save Sample, Search | confirmed |
| **Orig Note** of a sample, project **Key / Scale** | Sample → Tune → Shift+K1; Project → Key/Scale | confirmed |
| **Scale lighting** for 16 Levels Tune: Monochrome / Piano / Scale colours, Transpose16 | Sample → Tune/Play/**Lights** | partly confirmed |
| **Steps**: 16-step sequencer on the physical pads, grid 1/4…1/64 and triplets, velocity, mute steps, copy/paste | Shift+SEQ → B2 Steps | confirmed |
| **Events editor** with Chop / 16 Levels choice and audition, event move on the grid | Step Edit / Steps | confirmed |
| **Poly**: chords recorded and edited as separate notes | Seq | confirmed |
| **Search** samples and projects in Folder / Subfolders / Library, preview, load to a pad | Sample Select / Open Project → Shift+B3 | confirmed (basic) |
| **Batch Copy** of up to 16 sounds with automatic or manual targets | Sample → Shift+Erase → B2 | confirmed |
| **Resample** a pad (with Pad Link and Bypass) or a sequence to another pad | Shift+Pad 11 | confirmed |
| **Power key** asks, saves a separate copy of the project to SD, powers off | power key | confirmed |
| **Per-pad EQ**: one band, 6 types + Bypass, ±18 dB, live spectrum behind the curve | Sample → B3 Filter → Filter Env → **EQ** | confirmed |
| **Stereo → Mono** (left or right channel to a new pad) | Sample → Shift+B3 | trial |
| **Chords**: 8 scale degrees, 6/7/maj7/9, Maj/Min/Dim/Aug/Sus4, octave, inversion, latch, arpeggiator, chord repeat with Note Repeat; recorded as normal Tune events | 16 Levels Tune → Chop | trial (r24v) |
| **OptoCat/Warm**: optical-style compression, Gain −6…+6 dB, Shift Attack/Release/Knee; paw follows red GR, shared Bypass | Shift+Pad5 → B2 Type; B1 Warm | implemented; XPJ device retest pending |
| **AM Mod sequence pads**: Default + five colours; selected pulses once per bar at Stop, playing once per beat, queued twice per beat; visible background and stable brightness of other pads | Seq → Shift+K1 (AM Mod) | trial (r26q) |
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

> [!IMPORTANT]
> **Before installing or updating AM Mod, I recommend backing up all projects and samples from the SD card to your computer or another drive.** The installer's backup of `autorun/` does not include your projects or samples.

> [!IMPORTANT]
> **FAT32 / MS-DOS (FAT32) is available experimentally starting with r26e; exFAT remains supported.** FAT32 requires a package **r26e or newer** (use current r26s) and installer **1.0.2+**, or a manual copy of that package's complete `autorun/`. FAT32 has been checked in test images and the emulator; physical MPC testing is still pending. The published r26d and earlier packages require exFAT; updating only the installer does not add FAT32 support to an old package.
>
> **Use a card volume name such as `MPC` or `MPCSAMPLE`.** Only ASCII letters, digits, `.`, `_` and `-` are supported; spaces and Cyrillic letters in the card name prevent startup. This restriction applies to the card name, not your sample/project names.
>
> Check the filesystem before installing: Finder → select the SD volume → ⌘I → Format on macOS; drive Properties → File system on Windows. Installer **1.0.2+** checks the filesystem, card name and package compatibility before writing.
>
> **No reformatting is needed to install the current release on an existing FAT32 card.** Keep your projects and samples; replace the mod's `autorun/` folder with the complete new version. The installer backs up the previous folder and does not format or convert cards. FAT32 has a limit of **4 GiB minus 1 byte per file**.

You need an MPC Sample on firmware **1.3.0.12** (the mod checks the app's SHA-256 and
refuses to start on any other version) and a compatible SD card with a supported volume name: exFAT, or experimental FAT32 starting with r26e.

**Installer (Windows):** download `MPC-Sample-Mod-Installer.exe` and the
`am-mod-*.zip` package from [Releases](../../releases), run the installer, pick the zip
(or press *Latest release*) and the SD card, press **Install**.

**Installer (macOS / Linux / Windows with Python 3.8+):**

```bash
python3 installer/mpc_mod_installer.py                 # window
python3 installer/mpc_mod_installer.py cards           # list cards
python3 installer/mpc_mod_installer.py install am-mod-r26s.zip --card E:
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
   by itself (since r22 there is no extra question). Preparation time depends on the card and project. If a start
   ever hangs at 0 %, power on without the card, wait for the normal screen, then insert it.

   > **r27e shows Am MOD while preparing the app**, with the version and repository
   > address. Public r26s has a dark transition instead. Do not power off or remove
   > the card during preparation. The splash has no artificial minimum display time.

2. The mod checks the stock app and its own files before it starts; on any mismatch the
   stock app keeps running.
3. Work as usual. Save projects and samples to **External** (the SD card): in the mod,
   Internal storage is temporary and is lost when the mod stops.
4. In candidate r27e, save to External and use the power key. Remove the card only
   after shutdown. Start without the mod card to use stock. Public r26s still includes
   Project → Mod Off.

The power key also works in the mod: it asks, saves a project copy to the card and powers
off. Do not pull the card while the mod is starting, saving or shutting down.

## Try it without the device

[**Virtual MPC Sample**](https://github.com/orrpunk-art/virtual-mpc-sample) runs the stock
firmware in QEMU with the whole device in your browser, and runs SD autorun packages the
way the MPC does. The GIFs above were recorded there:

```bash
python3 lab.py autorun am-mod-r26s.zip --no-reboot --yes
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
revision. The current application entry is `sd-mod-session/build_chords_candidate.py`;
`verify_*.py` run scoped checks. These research scripts depend on frozen earlier
build outputs, verified preceding packages and your local firmware/rootfs; they
are not a standalone fresh-checkout build. r27e was assembled with
`package_empty_sample.py`. `refresh_release_docs.py` can refresh documentation
from an explicit verified base ZIP while asserting unchanged autorun bytes. ARM/QEMU checks (`verify_*.py`, `check_*.c`) use clearly marked
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

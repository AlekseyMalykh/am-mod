# Am MOD

**A track mixer, illustrated effects and multichannel USB for Akai MPC Sample.**

**English** · [Русский](README.ru.md) · [Download r30b](https://github.com/AlekseyMalykh/am-mod/releases/tag/r30b) · [All releases](https://github.com/AlekseyMalykh/am-mod/releases) · [Feedback](docs/FEEDBACK.md)

Am MOD boots from an SD card on firmware **1.3.0.12**. It adds eight tracks A–H, Master and Input FX, mixer resampling, tuners, chords, pad steps and workflow tools. The application runs in RAM; installed firmware is not rewritten. Boot without the mod card to use the stock app.

<table>
<tr><td width="50%"><b>Tape Delay</b><br><img src="docs/media/fx/tape-delay.gif" alt="Updated Tape Delay" width="100%"></td><td width="50%"><b>Shimmer Reverb</b><br><img src="docs/media/fx/shimmer-reverb.gif" alt="Shimmer Reverb" width="100%"></td></tr>
<tr><td><b>Amp Sim: guitar and bass voices</b><br><img src="docs/media/fx/amp-models.gif" alt="Amp Sim voices" width="100%"></td><td><b>Tape</b><br><img src="docs/media/fx/tape.gif" alt="Cassette Tape" width="100%"></td></tr>
</table>

*GIFs use the current production painter with synthetic PCM/meters and a substitute font. They demonstrate the UI and are not MPC recordings. [Gallery and regeneration](docs/FX_GALLERY.md).*

> [!IMPORTANT]
> Back up your projects and samples before installing. Save to **External**: Internal is temporary during a mod session. Only firmware **1.3.0.12** is supported. This is an unofficial project, unaffiliated with Akai/inMusic. [Notice](NOTICE.md).

## New in r30b

- **SD Card Access restored:** save a project copy, share the card as a USB drive, then **B3 Disconnect → B3 Return** without unplugging. Update the mod through that drive and restart to apply it.
- **USB audio:** stereo Master for phones/computers, or **18 channels: Master + eight stereo tracks A–H**. Channel names come from the MPC; no REAPER aliases are required. Stock stays stereo.
- **Updated effect screens:** tape transport/reels, Vinyl turntable, Shimmer room, Delay timing grids and amp cabinets. Bypass stops effect animations.
- **Dedicated K1–K3 layouts:** meaningful pages and Shift layers. Amp Sim adds seven guitar/bass voices alongside Original.
- **Smoother FX edits and a background sample tuner.** Unchanged processors retain their state; the user confirmed the earlier clicks disappeared.
- **Controls:** B2 Bypass in FX Chain, slot status, consistent Copy/Paste/Chain Bypass, ± track navigation, Duck/OptoCat DATA and Pickup/Scaled/Jump.
- **Route:** destinations creating direct or transitive send feedback are omitted.

[Full changes and verification limits](docs/RELEASE_NOTES-r30b.md).

## Quick start

1. Download `am-mod-r30b.zip` and the installer from the [release](https://github.com/AlekseyMalykh/am-mod/releases/tag/r30b).
2. Connect the SD card using a reader or **Project → SD Card Access → Connect**. For a first installation, use stock SD Card Access.
3. Select the ZIP and card in the installer and choose **Install**. Manually: replace the complete `autorun/` folder at the card root with the folder from the ZIP.
4. Safely eject the drive on the computer. In Am MOD, use **B3 → B3** to return; in stock firmware follow its USB disconnect prompt.
5. **Power the MPC off and on.** Copying an update does not replace the version already running in RAM.

exFAT is supported; FAT32 is experimental with files smaller than 4 GiB. Use an ASCII volume label containing letters, numbers, `.`, `_`, `-`, without spaces. The installer does not format the card, backs up the previous `autorun/` to `~/AM-Mod-Backups/`, and leaves projects/samples alone. That backup does not include your projects.

```bash
python3 installer/mpc_mod_installer.py install am-mod-r30b.zip --card E:
python3 installer/mpc_mod_installer.py status --card E:
```

On macOS use a mount point such as `/Volumes/MPC`; on Linux use the card mount path. [Installer guide](docs/INSTALLER-1.1.0.md) · [USB and updating](docs/USB_GUIDE.md).

## Choose a mode

**Project → Mod Settings → Quick → FX mode**, then restart.

| | Stock | Am MOD |
|---|---|---|
| Knob FX | Stock FX Select | A–H / Master / Input mixer |
| Processing | Stock path | Four serial insert slots per channel, sends and sidechain |
| USB capture | Stereo Master | Stereo Master or Master + A–H |
| Recording / resampling | Stock path | Input / Track / Master point, Wet / Dry |
| Stems | None | Song → Export → Stems to SD |

Am MOD does not run the project's stock FX Select in parallel with the mixer. Samples, pads and sequences remain the same in both modes. Defaults: Am MOD mode, Stereo USB.

## Mixer and effects

**Knob FX** opens the mixer. Pads **1–8** select A–H, **16** Master and **15** Input. **Pad FX** assigns sounds to tracks; unassigned pads go Direct to Master. The fader changes level; Mute mutes; B3 solos; B2 opens Route.

Pads **9–12** open the four FX slots. Processing runs top to bottom: **Slot 1 → 2 → 3 → 4**. Each effect and its analyser receive the preceding effect's output. Order matters, for example with Delay and Distortion. EQ shows its own response curve and the selected slot's live spectrum.

| FX control | Action |
|---|---|
| K1–K3 / Shift+K | Main / alternate layer |
| DATA | Parameter pages; select / open a slot in the list |
| B2 | Selected effect bypass |
| Shift+Erase / Shift+B2 | Copy / Paste |
| Shift+B3 | Chain Bypass |
| Shift+DATA in EQ | Add a point |
| Pads 1–8 / 15 / 16 | Open another channel's FX list |

[Amp voices](docs/AMP_VOICES.ru.md) are presets for the stock AIR Amp Sim, not measured circuit models or captured IRs. [Complete knob map](docs/FX_KNOB_LAYOUT.ru.md) · [User guide](docs/USER_GUIDE.md).

## USB: Master or individual tracks

**Mod Settings → Quick → USB audio → Stereo / Master + A–H**, then **restart the MPC** and reopen the DAW audio device.

| Stereo pair | Signal |
|---|---|
| 1/2 | Master |
| 3/4, 5/6, 7/8, 9/10 | Track A, B, C, D |
| 11/12, 13/14, 15/16, 17/18 | Track E, F, G, H |

For a phone, choose **Stereo** to record Master as before. Individual phone/adapter compatibility requires testing. In REAPER enable inputs 1–18 and choose pairs 1/2, 3/4, etc. Overlapping pairs 2/3 and 4/5 join adjacent mono channels; they are not additional stereo buses.

> [!IMPORTANT]
> **Avoid feedback:** if DAW playback returns through USB and is monitored by the MPC, turn off input monitoring on armed DAW tracks. Otherwise Master → DAW → USB Return → Master forms a loop.

SD Card Access stops the application and audio while the card is shared. USB drive access cannot run alongside mod playback/USB recording. [USB guide](docs/USB_GUIDE.md).

## Also included

- Pad/sequence resampling through the mixer; input recording through Input, Track or Master.
- **Stop+Chop** sample tuner; input tuner in Input ROUTE and Sample Record.
- Pad steps/chords, scale lighting, folder search, batch copy, pad typing and per-pad EQ.
- Screen Mirror, track colours and Pad Link / Mute Group lighting settings.

[User guide](docs/USER_GUIDE.md) · [Settings](docs/MOD_SETTINGS.md) · [Known limitations](docs/KNOWN_ISSUES.md) · [Changelog](CHANGELOG.md).

## Development

[Build and verification](docs/BUILD.md) · [Documentation index](docs/README.md) · [Contributing](CONTRIBUTING.md).

Building needs Linux/WSL, an AArch64 toolchain, your own firmware/rootfs and frozen inputs from earlier builds. This is not yet a self-contained clean-checkout build. Firmware and private projects are not published. The ZIP includes corresponding GPL source for the USB driver separately from the mod's source. [Notice](NOTICE.md).

Thanks to **Leusha** for testing and reports, the [Virtual MPC Sample](https://github.com/orrpunk-art/virtual-mpc-sample) authors for the development environment, and the community for feedback. [MIT licence](LICENSE); third-party components retain their licences.

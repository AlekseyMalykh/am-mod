# Am MOD r30b — USB, effects and controls

For **Akai MPC Sample stock firmware 1.3.0.12**. This release combines usb1–usb18 development and Sample Record fixes. Installed firmware is not rewritten.

## USB

- Project → SD Card Access is restored: save a project copy, stop the app, share the card as a USB drive and restore the project.
- After safe ejection in the computer OS, **B3 Disconnect → B3 Return** resumes without unplugging the cable. Audio is stopped during drive access.
- Stereo keeps Master L/R for computers/phones. Am MOD optionally sends **18 channels: Master + eight stereo tracks A–H**. Stock remains stereo. Profile changes require restart.
- MPC USB descriptors supply Master / Track A–H names. A RAM-only module targets the exact device kernel; corresponding GPL source is included under `SOURCE/usb-audio-names/` in the ZIP.
- Updating an existing mod over this drive has been tested on MPC. Restart to run the new version. Clean-card first installation and every phone/OS have not been separately confirmed.

[USB guide](USB_GUIDE.md).

## Effects and audio

- Updated Tape Delay/Tape transport, Vinyl, Shimmer room, Delay/Diff Delay, Spring, Phaser/Flanger and amp artwork. Bypass stops effect activity while track meters remain live.
- All 34 illustrated effects have dedicated K1–K3/Shift layouts, meaningful pages and reset mappings. Graphic EQ retains four points; Shift+DATA adds a point.
- Amp Sim: Original plus FD Clean, VX Chime, MR Crunch, MB Modern, SV Bass, BM Round and GK Punch. K1 selects a voice without resetting manual edits when staying in the same choice. Correct 11 native cabinet choices and Mono/Stereo. These are presets for stock DSP, not measured circuit models or captured IRs.
- Stop+Chop pitch analysis uses copied PCM on a low-priority worker; no borrowed native Sample/Program objects enter that thread. Cache identity includes sample, Trim, pad and project.
- EQ/own FX parameters are smoothed and unchanged processors retain tails/history. Changed native slots crossfade over 20 ms; their own tails can still change when rebuilt. The user confirmed the earlier clicks disappeared.
- Serial processing remains Slot 1 → 2 → 3 → 4. Tests verify final audio and second-EQ spectrum PCM after the first EQ; the user rechecked correct behaviour.

[Gallery](FX_GALLERY.md) · [Knob map](FX_KNOB_LAYOUT.ru.md) · [Amp voices](AMP_VOICES.ru.md).

## Controls and UI

- B2 Bypass in FX Chain; slot/state information below the track.
- Shift+Erase Copy, Shift+B2 Paste, Shift+B3 Chain Bypass across FX pages; pads open other channels' FX lists; ± selects A–H/Master.
- Send To omits self and direct/transitive feedback destinations, including grouped sends. Sidechain Key stays an independent detector.
- Duck/OptoCat initially select K1; DATA click cycles three columns forward and Shift+click reverses, with matching focus. Duck DATA changes real native Attack/Release/Amount and Shift In Boost.
- Separate Parameter, Knob FX and Fader Pickup/Scaled/Jump. Original waveform colours retained; Soft Clipper Drive shown in dB.
- Uniform 24×24 Project icons, FX category icons and compressor pad-light fixes.
- Am MOD hides stock Input Config → Rec Input Effects; Stock retains it.
- Sample Record B1 indicates the user's Off/Record/On choice rather than Rear's suppressed dry-monitor switch. Only Off is crossed out.
- Stop in Sample Record → Routing reaches the stock transport, allowing the second press to kill voices and the common handler to flush FX. Local recording stop/cancel remains.

## Verification and limits

Earlier MPC testing confirmed SD drive access, B3 return without unplugging, Windows/REAPER USB names, individual-channel recording and disappearance of clicks. This does not certify every feature of the final ZIP.

Host/ASan/UBSan and AArch64/QEMU coverage includes routing/cycles, FX controls/painter, serial EQs, pitch worker, actual native effect factories/DSP, preload, Mic/USB/Rear Input FX monitoring, the pinned Input Config callsite, monitor indication and Routing Stop. The names module was loaded under the original firmware kernel in a VM; physical USB audio was tested separately on MPC. GIFs use synthetic signals, not device capture.

**The new Input Config, indicator and Stop corrections, and the final r30b build, still need a hardware check after installation.** Many simultaneous effects, all cards/phones/OSes and latency are not guaranteed by lab tests. SD drive access stops the app. [Known issues](KNOWN_ISSUES.md).

## Install

Back up projects/samples. Install the complete `am-mod-r30b.zip`, or replace `autorun/`. Safely eject, return with B3 → B3, and **power the MPC off and on**. Save to External. [User guide](USER_GUIDE.md).

# r26x readiness audit

[Русский](RELEASE_READINESS-r26x.ru.md) · [Detailed tester list](TESTER_FEEDBACK_LEUSHA.ru.md) · [Known issues](KNOWN_ISSUES.md)

**2 November 2026. A stable release is not ready: important reports remain open.**
The public release is [r26s](https://github.com/AlekseyMalykh/am-mod/releases/tag/r26s).
r26w was the preceding device-test package on SD. r26x is a local, unpublished
candidate; its new changes are not confirmed on an MPC.

## Status meanings

- **Hardware-confirmed:** K end stops fixed in r26i, confirmed by the author;
  [#2 closed](https://github.com/AlekseyMalykh/am-mod/issues/2). Earlier author
  confirmation covers Sample playback hue; Leusha noticed the Green/Cyan correction.
- **Implemented/automatically tested:** code and specified tests exist; this does
  not establish every original user scenario or later hardware combination.
- **Open/partial:** an original scenario or a material check remains.
- **Proposal:** not implemented, or only part of the idea exists.

The supplied list retains1–20,21a–d and added22: **25 tracking rows**, including
questions, suggestions and related cases, not25 independently confirmed bugs.

## Report matrix

| Reports | r26x status |
|---|---|
| L01 | Partial: complete XPJ opening/modal gating fixed; full A/B/empty → mod → Mod Off → stock → restart cycle pending |
| L02–L03 | Open: project/sample load errors around exit/restart; exact paths/sequence needed |
| L04 | Open: active project identity and ordinary Save destination after exit copy |
| L05 | Erase-based Delete implemented; affected browser/search state not yet checked |
| L06 | Open: first Chop missing its first-pass Play flash; original video not reproduced in this audit |
| L07, L10, L12 | Fixed with the hardware confirmation scoped above; Standard deliberately uses native colours |
| L08–L09, L22 | Workflow/compressor hues and Copy/Extract colour history/Undo/Redo/rollback implemented; original MPC bank/Save/Load cases pending |
| L11 | Open: muted waveform; previous full VM showed grey waveform and did not reproduce original case |
| L13 | Partial: Batch reverse Shift+Bank exists; native Copy original case pending |
| L14 | Quick-start Lights instructions added/checked against code; physical walkthrough pending |
| L15 | Proposal: extra Key/Scale access from Input Config not implemented |
| L16 | Agreed Seq rhythm implemented; Standard offers native alternative; MPC queue/cancel/readability pending |
| L17 | Open investigation: low-frequency spectrum resolution not measured |
| L18–L19 | Proposals: spectrum brightness control and Pad Colour waveform/spectrum tint not implemented |
| L20 | Spectrum across B3, retained native curves and later EQ side-strip restoration implemented; author confirmed curve restoration |
| L21a | No-Shift K2 Pad Colour implemented; deliberately disabled in Standard |
| L21b | Arbitrary RGB editor not implemented |
| L21c | Partial: OptoCat/Warm/sleep/GR implemented; Type selects algorithms, no independent character picker |
| L21d | Artwork-help offer, no agreed asset task; public thanks in r26s, separate reply remains a draft |

## Changes since public r26s

1. r26t–v: double white selected-source pulses in Batch Copy/Extract/Mono/Resample;
   assigned empty targets green, occupied red, unselected empty destinations off.
2. Default + five user colours: Blue, Violet, Magenta, Orange, Pink. System hues
   reserved; legacy IDs retained without renumbering.
3.400ms white/400ms identity focus in Sample/Chop/Sound; grey recorded-event
   flashes on muted instruments without enabling audio.
4. r26w: global Standard / AM Mod preference on SD; seven Project entries use
   the native six-row viewport, Mod Off last.
5. Note Repeat owns compressor B1–B3, including a latched overlay.
6. r26x: ignore queued hidden Repeat paints; request one header restore after
   close. Regression checks subsequent Type frames without a repaint loop.
   This reproduces/corrects a specific test path; the original intermittent
   hardware flicker is still awaiting confirmation.
7. r26x: **16 Levels Lights/Piano/Scale and musical colours work in Standard**,
   with tonic/scale/Transpose16/role colours. Pad/Seq Colour remain disabled there.

Preserves OptoCat/Warm and previous fixes: Gain −6…+6 dB, Shift controls,
paw/red bar sharing real GR, input-driven bow, clear sleeping meters/background,
shared Bypass and exclusive compressor algorithms.

## Verification and limits

ARM/QEMU normal/UBSan cover native K/FIFO/Scaled, compressor factory/DSP/painter,
pad controls/LEDs/Copy/Extract history, EQ/native side strips, project restore/exit
copy, native resample, global lighting preferences and the actual seven-row
Project model. Source SHA-256 checks prevent stale proofs. The production build
checks the complete preload constructor over frozen patches.

New r26x checks cover hidden Repeat paints, stable Type after release, Standard
Lights/Transpose16 controls and identical Piano/Scale LED values in both modes.
Some native generator, GUI and audio boundaries are fixtures. This is **not a new
full native-main run, a physical MPC trial or a FAT32 card validation**.

## Required before a stable release

| Check | Pass condition |
|---|---|
| L01: distinct test copies A/B and empty stock | Correct own sequences/notes/samples/tempo/mixer/FX after startup/restart; expected saved XPJ chosen |
| L02–L03: Mod Off → stock → another project → restart | No project/sample errors; completed exit copy and resolving sample paths |
| L04: ordinary Save after each transition | Clear name/path, originals intact, no silent exit-copy identity substitution |
| OptoCat/New Project | Repeated New Project, both types, Warm/Bypass; no unavailable OptoCat message |
| Note Repeat | Held/latched and fast release; B1–B3 select divisions; stable Type without spontaneous changes |
| Project/Pad Lighting | All seven rows scroll, Mod Off last, Cancel works; preference survives restart |
| 16 Levels | Lights/Piano/Scale/Transpose available in both modes, settings survive XPJ round trip |
| Workflow palettes | Copy/Batch/Chop/Extract banks/targets/Undo/Redo/Mute; selections readable; first Chop tested separately |

FAT32 remains **experimental**, implemented from r26e and included in public
r26s, without physical FAT32 confirmation in available reports. Closed
[#1](https://github.com/AlekseyMalykh/am-mod/issues/1) and a successful exFAT
format workaround do not prove FAT32 startup. An exFAT-scoped release can retain
this explicit experimental limitation.

After a short hardware smoke test, a **prerelease** with these open reports can
be considered separately. Nothing was published during this audit. Public
[pinned #3](https://github.com/AlekseyMalykh/am-mod/issues/3) still describes r26s
and should be synchronised when publishing the next version.

Other limits: session-only OptoCat settings, temporary Internal storage,
newest-saved rather than last-opened XPJ startup, possible loss of the mod
extension on stock resave. Name-keyed Lights rename migration and dedicated
new Song/Loop status schemes remain unimplemented.

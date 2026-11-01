# Changelog

## r26h candidate — Scaled endpoints and OptoCat (issue #2)

- Correct exact Scaled limits for first/reset or repeated-endpoint events through the native range/skew/quantisation and setter. Interior motion, disabled bindings, Pickup and Jump remain unchanged.
- Preserve the complete OptoCat compressor from `81189e0`, including DSP, animation and Type controls, in both candidate builders.
- Execute production permanent K rendering and the native display timeout between real takeover calls: no direct parameter/history/focus interference in the isolated fixture. Full GUI timing and hardware input remain unverified.
- Add bounded endpoint diagnostics. ARM/QEMU and UBSan cover native takeover, rendering, routing with OptoCat, compressor DSP/animation/native adapter and the full preload constructor. Hardware confirmation remains pending. [English](docs/RELEASE_NOTES-r26h.md) · [Русский](docs/RELEASE_NOTES-r26h.ru.md).

## r26g candidate — preserve Scaled/Pickup takeover history (issue #2)

- Replace r26f's last-value-only knob merging: preserve each knob's initial physical position and all direction changes, merging only later points within the same monotonic movement. Scaled and Pickup need that history to reach their limits and detect pickup crossings.
- Actual firmware takeover code reproduces the r26f failure: a direct sweep reaches 0, while r26f's queue leaves the parameter at 45%. The corrected queue matches direct native processing for Scaled, Pickup and Jump, both limits, direction reversals, three knobs and five initial parameter values, with and without UBSan.
- Preserve action/context boundaries and final knob focus. Retain the earlier protection against overflowing the queue on straight sweeps. No takeover setting or native takeover algorithm is changed.
- Full preload constructor validation and input regressions pass. Hardware verification remains pending. Includes experimental FAT32 support. [English](docs/RELEASE_NOTES-r26g.md) · [Русский](docs/RELEASE_NOTES-r26g.ru.md).

## r26f candidate — knob endpoints (issue #2)

Superseded by r26g: last-value-only merging omitted history needed by Scaled/Pickup. The r26f tests recorded raw parameter inputs and did not execute native takeover processing.

- Fix a reproduced input-queue overflow where a fast K1/K2/K3 sweep delivered its newest value immediately, then older queued values overwrote the endpoint. Retain the latest absolute positions and their final focus order within the trailing knob run.
- Preserve button/pad actions, relative encoder increments and ownership/source/generation boundaries. The physical Main Volume path is unchanged.
- ARM and UBSan checks reproduce the old failure (0 became 28), verify both endpoints on all three knobs, exercise 50,000 concurrent updates and check naming/resampling input routes. Full preload constructor validation passes. Warp verification on a physical MPC remains pending; the issue is not closed by fixture tests.
- Includes r26e's experimental FAT32 support. [English](docs/RELEASE_NOTES-r26f.md) · [Русский](docs/RELEASE_NOTES-r26f.ru.md).

## r26e candidate — experimental FAT32 support

- Add experimental FAT32 support alongside exFAT in native autorun, the session controller, sandbox checks, diagnostics and the ARM save/journal flush helper. Pin the detected filesystem and retain canonical path, device/inode and write-scope checks. Physical MPC FAT32 testing is pending.
- Installer 1.0.2 accepts FAT32 on Windows, macOS and Linux. It rejects older exFAT-only packages on FAT32 before backup or writes; existing projects and samples do not need reformatting.
- Preserve the r26d application library and its pad-colour, project restoration and Filter/EQ changes. FAT32's per-file size limit still applies.
- Validation and hardware status: [English](docs/RELEASE_NOTES-r26e.md) · [Русский](docs/RELEASE_NOTES-r26e.ru.md).

## r26d — 1 November 2026

User-facing list of fixes and new features: [English](docs/RELEASE_NOTES-r26d.md) · [Русский](docs/RELEASE_NOTES-r26d.ru.md).

- Fix the r26c regression that removed the filter and envelope curves together with the waveform. Suppress only native waveform rasterisation within Sample Filter/Filter Env; keep the native graph renderer and its curve stages, with a dim spectrum overlay.
- Trim, Tune and unrelated waveform views retain their native rendering. ARM/QEMU and UBSan checks cover the selective hook, relocated native prologue, argument forwarding, scope restoration and unchanged curve stages. The author approved r26d after the device trial.
- Keep spectrum capture active while Filter, Filter Env or EQ is visible, including after silence; stop capture when leaving those pages.

## r26c — device trial, 1 November 2026

Superseded by r26d: device testing found that the full graph clear also hid the filter/envelope curves.

User-facing list of fixes and new features: [English](docs/RELEASE_NOTES-r26c.md) · [Русский](docs/RELEASE_NOTES-r26c.ru.md).

- Replace the waveform, playhead and time ruler on Filter and Filter Env with an opaque spectrum and frequency labels. Clear the old graph on every paint, including silence. Native headers and parameter knobs remain available; EQ is unchanged.
- Device testing found that this graph correction also hid the native curves; r26d replaces it.

## r26b — device trial, 1 November 2026

- Open XPJ files as full projects during startup and project search. The browser callback's Boolean is `asKit`; passing `true` retained the previous project's sequences and mixer settings. Use `false` on the native UI path and retry startup until the browser callback exists.
- Sample → Mix: K2 selects Pad Colour without Shift; Shift+K2 keeps native Kit Volume and K1 keeps Volume. Insert Pad Colour into the encoder focus cycle. Green has no blue component.
- Sample playback flashes keep each source pad's assigned colour and bank; Stock, Levels/picker and Steps keep their existing semantics. Button LED CC IDs no longer masquerade as pad RGB updates.
- Show the selected pad's spectrum on Filter and Filter Env as translucent bars within the native graph, alongside the existing EQ spectrum.
- ARM/QEMU and UBSan regressions pass. Full native emulator tests restore three distinct B events, tempo and mixer/track volumes over an empty A; sampled playback RGB stays red at changing brightness. The author confirmed pad colours on the device; these changes continue in r26d.

## Installer 1.0.1 — 1 November 2026

- Stop installation before any writes on FAT32/MS-DOS, an unverified filesystem or an unsupported SD volume name; the current mod requires exFAT and an ASCII name without spaces.
- Read the actual filesystem and volume name on macOS with `diskutil`, and the device label on Linux. Unlisted paths no longer bypass metadata checks.
- Keep GUI validation errors visible and show the SD requirements at startup. Add regression tests on Windows, macOS and Linux.
- Add prominent English/Russian installation requirements. The r26a mod payload is unchanged; FAT32 support has not been added.

**English** · Russian: [docs/FEATURES.ru.md](docs/FEATURES.ru.md), [sd-mod-session/IMPLEMENTATION_HISTORY_RU.md](sd-mod-session/IMPLEMENTATION_HISTORY_RU.md)

All builds target MPC Sample firmware 1.3.0.12 and run from the SD card; none of them writes the firmware. Details of each build are in `sd-mod-session/R*_RU.md` (Russian). "Confirmed" means checked on a real MPC Sample.

| Build | Date (2026) | Changes |
|---|---|---|
| **r26a** | 1 Nov | Other Sounds background in Steps dimmed to native RGB080808; empty steps remain off and own Pad Colour events take priority. Real native colour conversion plus sparse/dense context verified under ARM/QEMU and UBSan. Published at the author's request; automated and hardware coverage have separate scopes. |
| **r26 (development)** | 1 Nov | Steps screen outline, numbering and event-state labels now use the same physical-to-chronological mapping as the pad fill. Encoder cursor toggle also hides the outline. Production drawing checked for all16 pads on two pages under ARM/QEMU and UBSan; device confirmation pending. |
| **r25z (development)** | 1 Nov | Steps owns both RGB and indexed pad LEDs, removing the mirrored native cursor; empty steps off, other sounds dim white and visible by default. Events B3 is red Erase for the selected row; Erase+pad keeps native sequence-wide confirmation. ARM/QEMU and UBSan verified; device confirmation pending. |
| **r25y (development)** | 1 Nov | Pad Colour limited to sound layouts so FX Select keeps native status colours; per-sound colours in Events names and sound pads, and in Steps events/Sound selection. Selection, playhead, Mute and Erase keep priority. ARM/QEMU checks pass; device confirmation pending. |
| **r25x** | 1 Nov | Public release of the author-confirmed r25w functionality; temporary Erase-route and project-open callback diagnostics removed; unnamed-storage diagnostic absent; English/Russian README and FEATURES updated; release/source privacy audit. |
| **r24y–r25w** | 31 Oct–1 Nov | Per-pad Pad Colour on Sample → Mix → Shift+K1 with encoder focus and project persistence; project/folder Erase confirmation and companion-data cleanup; one Mod-Exit-Last project; newest-project autoload; Internal/External labels; three Lights modes; framed Shift badges and text-cursor fixes. Author confirmed current functions on 1 Nov. |
| **r24x** | 31 Oct | Search results light the pads with the shared palette: target white, red and blinking when it would replace a sound, occupied pads dim white. |
| **r24w** | 31 Oct | The mod is now called **AM Mod**: packages `am-mod-*.zip`, `AM-Mod-Installer.exe`, start screen "AM Mod". No functional changes from r24v. |
| **r24t–r24v** | 31 Oct | Resample target chosen on a 4×4 bank grid (source outlined, overwrites red). **Live spectrum** of the edited pad behind the EQ curve (dim amber from r24v). |
| **r24q–r24s** | 31 Oct | **Pan** and Tune bars grow from a centre mark. **Block layout** for the Chords, Steps, Stereo → Mono and Resample windows and the Steps edit menu. Knob FX works inside Chords. |
| **r24k–r24p** | 31 Oct | Seq pads: **Stock** colour by default, the playing sequence blinks once per beat and the queued one twice, an empty queued pad stays visible; with recording armed the Seq screen stays and the pads show the Sample layout. **Note Repeat** in Chords repeats the held chord at the stock repeat rate. Queued buttons are passed to the stock app in step with the audio (crash on Stop fixed). |
| **r24e–r24i** | 30–31 Oct | Chords **arpeggiator** on K3 (Off by default; Shift+K3 rate 1/4…1/32, Shift+K1 range); plays only while roots are held. **Stop pressed twice** no longer crashes: releasing all voices waits for the audio render. Tap Tempo works in Chords; Shift no longer looks held on entry. Chord playback lights were tried and removed. |
| **r24b–r24d** | 30 Oct | Chords: physical pad releases, stock Poly switched on at entry. One meaning for pad colours across Steps, the event editor, Batch, Mono and Resample. Mod-Exit copies named by date and time. Crash backtraces in the card log. |
| **r24a** | 30 Oct | Chords entry fixed: background Sample observers no longer block it. |
| **r24** | 30 Oct | **Chords** mode in 16 Levels Tune → Chop: scale degrees, 6/7/maj7/9, Maj/Min/Dim/Aug/Sus4, octave, inversion, Latch; recorded as separate Tune events. |
| **r23a** | 29 Oct | Resample includes Pad Link and Bypass (confirmed). Confirmed: Poly recording, boot with the card inserted, Orig Note save/reopen. |
| **r23** | 29 Oct | **Power key**: stock question, project copy to SD, power off (confirmed). Sequence lighting confirmed. |
| **r22–r22c** | 28–29 Oct | Project copy to SD on Mod Off; autorun without an extra question; **Poly** chord recording as separate events; 12 sequence colours (Shift+K1 in Seq). Test boot animation removed in r22a. |
| **r21–r21d** | 28 Oct | Several notes in the Tune event editor; note entry on empty Steps; **Stereo → Mono** (Shift+B3); amber Note Repeat and Knob FX panels; lighting fixes. |
| **r20–r20g** | 27–28 Oct | Fast resample of a single pad with its processing (confirmed in r20g). |
| **r19–r19d** | 27 Oct | **Pad EQ** page Sample → Filter → Filter Env → EQ (audible, confirmed in r19c); stock Seq/Sample resample entry. |
| **r18–r18a** | 26–27 Oct | Experimental FX chain for resampling; later withdrawn in favour of the stock FX Select and the pad EQ. |
| **r17–r17d** | 26 Oct | Resample entry (Shift+Pad 11); text cursor, Backspace/Delete in pad typing; Steps "Other Sounds"; Lights in the Tune event editor; playback blinking in Sample/16 Levels/Chop. |
| **r16–r16d** | 25–26 Oct | Readable Steps, Copy/Extract and search screens; audition in the Chop / 16 Levels event editor (L confirmed). |
| **r15–r15h** | 25 Oct | **Batch Extract** (confirmed in r15c); C/L event editor; preview volume (K3); copy with events; Lights Degrees; search auto-preview; Erase = stock Delete in User; Mod Off moved to the end of Project with a confirmation. |
| **r14–r14f** | 24–25 Oct | **Batch Copy** of up to 16 sounds (confirmed in r14e); **search** in Sample Select / Open Project; Steps mute. |
| **r13–r13g** | 24 Oct | **Steps** on the physical pads; moving events along the grid in Step Edit; Copy/Paste of events. |
| **r12–r12b** | 24 Oct | Project **Key / Scale**, **Lights** page, mod settings saved in the project; unified top buttons and tabs. |
| **r11** | 24 Oct | **Orig Note** of a sample. |
| **r10–r10b** | 23–24 Oct | **Typing names with the pads** (phone keypad). |
| **r9–r9h** | 23–24 Oct | Saving to **External** (SD) from inside the mod; Mod Off. |
| **r8–r8a** | 23 Oct | Redesigned **K1–K3 panel**: names and values always visible, amber focus, S marks. |
| **r3** | 23 Oct | First display patch: K1–K3 values (`sd-k-value-peek`). |
| **clone trial** | 21–23 Oct | Loader: verified copy of the stock app runs from RAM, stock app restored on exit or failure (`sd-clone-trial`). |

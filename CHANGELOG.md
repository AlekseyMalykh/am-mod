# Changelog

## r27q — candidate, unpublished

- **Degrees layout** for 16 Levels Tune: pads play consecutive key notes; original note, I, IV and V in separate colours (V stored as MPCS18 when changed); K1 on the Lights page is now called Layout.
- **Search:** live top matches, match count, Shift+Erase clears, the last query is restored, five-row list with folder line.
- **Rear Routing, Mod Settings and confirmations** redrawn in the stock list/panel/frame style.
- **Splash:** original “Am” with “MOD” at the same height; anti-aliased version and repository text.
- Clearer error messages and Turn / Press hints.

[English notes](docs/RELEASE_NOTES-r27q.md) · [Русский](docs/RELEASE_NOTES-r27q.ru.md).

## r27p — release

- **Rear recording and monitoring.** Sample Record → B2 Routing: K1 chooses Record Rear L / Record Rear R / Record L + R. Press a pad to record mono. For L + R, choose the L pad, then a different R pad; recording starts simultaneously after the second choice. B1 or STOP cancels the first choice; occupied pads require confirmation. STOP finishes recording. Hold Pad Bank and use pads 1–8 for banks. Standard stereo recording on one pad remains in the normal Sample Record window; Routing is offered only for the Rear source.
- **Monitor without recording.** Input Config → Shift+B2 Routing: B2/B3 toggle L/R monitoring, K2/K3 set pan, Shift+K2/K3 set monitor level. The same controls work in recording Routing. Input Config pad presses never start recording. Monitor gain/pan do not change the dry mono recordings. Settings are saved with the project.
- **Knob FX priority.** Physical K1–K3 control Knob FX while it is open, including over custom screens. Routing resumes afterwards. Note Repeat keeps its controls.
- **Scale and Orig Note.** Setting Orig Note selects Scale. Tune lighting resolves the current sample from the project model, including when a different pad is selected in Compressor and across banks A–H.
- **Menu clicks.** A fast encoder click after leaving Mod Settings is passed to the current native menu. Hidden Sample controls no longer take Project controls.
- **Copy and EQ.** Hold Erase after entering Copy with Shift+Erase; Shift+Pad Bank switches banks backwards. Returning from Seq to Sample retains the EQ page.

[English notes](docs/RELEASE_NOTES-r27p.md) · [Русский](docs/RELEASE_NOTES-r27p.ru.md).

## r27n — prerelease

- **Preview from end.** Hold **STOP** and press a pad in Sample or Chop to hear the end of the trimmed sample or slice. In **Project → Mod Settings → B3 → Sample → Preview from end**, choose 100–4000 ms. The default is 1000 ms. The setting is saved with the project and in card defaults.
- **Steps.** K1 sets the sequence length in bars, K2 sets the grid, and Shift+K1 is free. Use **SOUND + pad** to choose the sound and **PAD BANK** to switch banks. Available empty steps are dim, steps with events are brighter, and pads beyond the end stay dark. Mute shows audible steps in green, muted steps in red and mixed steps alternating.
- **Sample lighting.** A playing loop flashes in its assigned colour after you select another pad. Pressing SHIFT alone keeps the pad brightness. While using Mute, audible and muted pads are green and red. Muted pads have a grey waveform and spectrum, with filter/EQ curves and envelopes still visible.
- **Palette.** Eight assignable colours plus Default for samples and sequences. Six separate system colours remain swappable in Mod Settings.
- **Copy Pad and recovery.** Temporary audio uses a dedicated SD workspace to avoid false “Insufficient space on drive” errors with large samples. A successful named Save is recorded for project recovery.

[Release notes](docs/RELEASE_NOTES-r27n.md) · [Список изменений](docs/RELEASE_NOTES-r27n.ru.md).

## r27f — prerelease

Fixed the missing temporary ProjectData folder that prevented loading samples
from the built-in library/search, Copy Pad and assigning rendered sounds or
sequences. The author confirmed the correction on MPC Sample. Prepare the
shared audio workspace across startup, Load, New Project and recovery identity
changes while preserving existing audio and native project/autosave paths.

The splash now reads `version: r27f`; About and the boot helper use the same
shared version. Includes all r27e changes, which had not been publicly released.
Installer 1.0.3 replaces the obsolete Mod Off shutdown hint.
[Release notes](docs/RELEASE_NOTES-r27f.md) ·
[Описание исправления](docs/RELEASE_NOTES-r27f.ru.md).

## r27e — prerelease

Consolidates all changes since public r26s: Mod Settings/Standard/AM Mod,
palette/tint/pulse defaults and project snapshots, source/target and empty-pad
cursors, native browser load-target selection, supported Pad Bank+K reset,
recovery identity/first Save and missing Temp-Project autosave handling, first
native startup Load/DRM splash/About, normal shutdown without Mod Off, exclusive
Steps WRITE/TOGGLE, browser ERASE, Repeat priority and drawing fixes.
Preserves the confirmed K end-stop fix, OptoCat/Warm and earlier musical tools.
Hardware scenarios and future requests remain explicitly open.
[Full English changes](docs/RELEASE_NOTES-r27e.md) ·
[Полный список](docs/RELEASE_NOTES-r27e.ru.md).

## r27d — local browser cursor fix, unpublished

White browser selection follows the native load-target index, including empty pads and all banks; search retains the current destination. Steady/blink settings are preserved.

## r27c — local candidate, unpublished

- Fixed delayed Auto Save Error after New Project: the first autosave uses
  the native serializer to create the missing Temp-Project.xpj from the
  current project, without importing the previous project's data.
- White library/Copy/Batch/Chop/Mono/Render selection now follows Selected
  Pad → Cursor pulse: Steady / Slow / Normal / Fast. Assigned colours show
  between flashes; empty selected pads turn off between flashes.
- Hold Pad Bank and turn K to reset that parameter to its factory default;
  hold Pad Bank, then Shift, and turn K for its Shift parameter. Further
  turns are consumed until Pad Bank is released. Native held-Pad-Bank
  selection using pads 1–8 is preserved.


## r27b — prepared test release, unpublished

- Use compact 8-pixel version/repository text near the bottom of the splash.

- Shared version/repository on the DRM and in-app splashes and read-only About (5/5).
- Circular five-tab navigation; About cannot reset or edit settings.
- Log real splash duration without adding a minimum display delay.
- r27a splash is now confirmed visible on MPC; preserve the validated DRM handoff.
- Consolidate all changes since public r26s and separate remaining device checks.
- [Changes](docs/RELEASE_NOTES-r27b.md) · [Русский](docs/RELEASE_NOTES-r27b.ru.md).

## r27a — local DRM-display candidate, unpublished

- Replace fbdev splash writes with an owned DRM framebuffer, explicit active
  scanout verification, master release and automatic native-frame handoff.
- Bound resource allocation/cleanup and preparation cancellation; repeated
  display starts are idempotent. Application library remains byte-identical.
- Native VM QMP output matches the owned image; handoff/cancel pass. Physical
  MPC visibility remains pending after the failed r26y/r26z fbdev trials.
- [Changes](docs/RELEASE_NOTES-r27a.md) · [Русский](docs/RELEASE_NOTES-r27a.ru.md).

## r26z — local display-fix candidate, unpublished

- Address the r26y device report of a black boot screen: blank/unblank before
  framebuffer transfer, sequential writes, explicit layout/reset/error logs.
- Start early recovery via an acknowledged setsid watcher; retain systemd fallback.
- Retain the exact r26y application library, one native SD Load, project recovery,
  OptoCat/Warm, settings and all previous fixes. Physical splash retest pending.
- [Changes](docs/RELEASE_NOTES-r26z.md) · [Русский](docs/RELEASE_NOTES-r26z.ru.md).

## r26y — local candidate, unpublished

- Replace Pad Lighting with four Mod Settings tabs: optional waveform/spectrum tint, service/user palette swaps, cursor timing, confirmed per-section recommended reset (B1 Cancel / B3 Restore) and encoder scrolling. Keep five user slots and Standard Piano/Scale.
- Persist display settings and OptoCat Type/controls/Warm in optional XPJ data; retain native Duck/Bypass. Restore on successful reads, preserve state on failure, support legacy projects and reset Opto on New Project.
- Preserve recovery project identity across asynchronous serialization, retain recovery sample paths and the native Temp-Project autosave destination, and require a separate first-Save confirmation. Remove Project Mod Off while keeping normal-power copy saving and emergency recovery.
- Early Am MOD splash before application preparation, independent early rollback and SD selection in the first native full Load; disable late duplicate autoload while retaining Temp-Project autosave and manual Load. Physical startup timing is pending.
- Exclusive SEQ RECORD WRITE and SAMPLE RECORD TOGGLE in Steps. Retain K, OptoCat/Warm, browser ERASE and prior workflow fixes.
- [Changes](docs/RELEASE_NOTES-r26y.md) · [Русский](docs/RELEASE_NOTES-r26y.ru.md) · [Readiness](docs/RELEASE_READINESS-r26y.md). Public download remains r26s; device confirmation is pending.

## r26x — local candidate, unpublished

- Keep 16 Levels Lights controls and musical Piano/Scale layouts available in Standard and AM Mod; preserve tonic/scale/Transpose16 and role colours. Standard still disables Pad Colour/Seq Colour controls and identity overlays.
- Ignore queued paints from a hidden Note Repeat overlay; restore the compressor header once after Repeat closes. Add regression coverage for hidden paints and stable Type across subsequent frames. Original MPC fast-release confirmation is pending.
- Audit README, feature/colour maps and all tester report statuses. Separate implemented, device-confirmed, open and proposed work; retain L01–L04 as stable-release gates. Public download remains r26s. [Readiness](docs/RELEASE_READINESS-r26x.md) · [Русский](docs/RELEASE_READINESS-r26x.ru.md).

## r26w — local candidate

- Add Project → Pad Lighting: global Standard / AM Mod preference; retain project colour data.
- Keep Mod Off last in the native seven-entry menu with a six-row viewport.
- Give Note Repeat B1–B3 priority over Type/Warm, including latched overlay and queued release.
- Scope and controls: docs/RELEASE_NOTES-r26w.md.

## r26v — local candidate

- Keep empty unassigned destination pads off; show red/green only on assigned targets.
- Give Sample/Chop/Sound selection a clear400ms white/identity cursor and reset selection phase.
- Preserve muted-event flashes, Seq rhythm, native K and integrated OptoCat/Warm.
- Device candidate scope: docs/RELEASE_NOTES-r26v.md.

## r26u — local candidate

- Separate assignable identities from reserved service colours; add Pink without renumbering legacy IDs.
- Show destination occupancy with red/green service colours and selected source double flashes.
- Add Sample focus/Mute activity snapshots, Chop focus and Steps erase precedence; preserve Seq rhythm and OptoCat.
- Scope, compatibility and pending hardware checks: docs/RELEASE_NOTES-r26u.md.

## r26t candidate — visible pad selection across workflows

- Pulse selected Batch Copy sources and Batch Extract slices between white and their assigned/source hue, including the reported all-purple layout; unselected pads stay steady. Selected white pads pulse in brightness. Preserve red overwrite warnings and metadata.
- Recognise the exact native Copy Pad source marker using its validated UI mode; retain Mute/browser warnings and exclude sequence-copy mode from sound-pad recolouring. Add source pulses to Mono/Resample and white/base-colour cursors to Chop/Tune event pickers. Keep native ordinary Chop, Sound and Seq markers.
- Verify actual native Copy selected-source bitsets, purple Batch pages/eight banks, all colours, picked slice values, bank/preview/Undo and unchanged audio routing in ARM/QEMU/UBSan. Preserve r26s EQ strips and OptoCat Gain −6…+6 dB. [English](docs/RELEASE_NOTES-r26t.md) · [Русский](docs/RELEASE_NOTES-r26t.ru.md). Local device-test candidate; unpublished.

## r26s — public release, 2 November 2026

- Combine r26e–r26r: experimental FAT32/installer1.0.2, confirmed native K fix, OptoCat/Warm and Shift controls, shared Bypass/master rebinding, project-start gating, Copy/Extract colour history, compressor/mute colours, Seq pulses and Sound cursor.
- Restore native EQ volume/pan indicators by limiting the overlay to the centre; preserve assigned hues in ordinary Copy/browser and Mono/Resample/Extract palettes. Set OptoCat Gain to −6…+6 dB, default0.
- Keep remaining project-error, navigation and waveform cases separate from implemented fixes. Thank Leusha and other testers. [English](docs/RELEASE_NOTES-r26s.md) · [Русский](docs/RELEASE_NOTES-r26s.ru.md). Earlier candidate entries record the verification status at their time.

## r26r candidate — paw follows GR, Sound cursor reveals colour

- Drive the OptoCat paw from the same real gain-reduction snapshot as the red GR bar and number. Keep the bow on dry input level. Sustained compression holds the paw down; recovery returns it. DSP Attack/Release govern both without a second gesture envelope or threshold/hit trigger. Preserve audio DSP, gain bounds, Warm, artwork and sleep.
- While holding Sound in Steps, alternate the selected pad between white and its assigned hue (240 ms white / 400 ms colour). Preserve other pads, selection/bank mapping and empty-slot indication.
- ARM/QEMU/UBSan verify actual DSP-to-paw timing, no false strikes and Sound cursor phases across colours/banks; physical trial pending. Preserve r26q Seq pulses and previous fixes. [English](docs/RELEASE_NOTES-r26r.md) · [Русский](docs/RELEASE_NOTES-r26r.ru.md). Local candidate.

## r26q candidate — tempo-synced sequence pad pulses

- Pulse the selected Seq pad once per bar when stopped, the playing pad once per beat and a queued target twice per beat. Keep the assigned hue and visible background between short flashes; return to the normal beat rate after handoff.
- Keep other occupied selectors at the same brightness before and after Play. Read native tempo and time signature on the UI thread; interpolate the engine clock while playing and use a tempo-driven clock while stopped. Preserve empty/selected/queued, bank, recording and Mute ownership.
- Preserve r26p shared Bypass/master lifecycle, OptoCat/Warm and previous fixes. ARM/QEMU/UBSan verify pulse timing, colour, tempo/meter and transitions; physical trial pending. [English](docs/RELEASE_NOTES-r26q.md) · [Русский](docs/RELEASE_NOTES-r26q.ru.md). Local candidate.

## r26p candidate — compressor lifecycle and shared Bypass

- Rebind OptoCat to replacement native master processors after New Project; keep the shared hook table immutable and reject handoff until an in-flight block finishes. Late old callbacks/destructors cannot retire the new master.
- Use the native Bypass parameter for both types and retain native B3 handling. Type browsing while bypassed keeps processing disabled; waking enables only the selected algorithm. Opening the page shows the active type, or Duck when both are off.
- Preserve Warm, artwork, sleep display, meters, +6 dB cap, K and pad-colour fixes. ARM/native-factory and UBSan checks cover repeated replacement and routing; physical New Project/B3 verification is pending. [English](docs/RELEASE_NOTES-r26p.md) · [Русский](docs/RELEASE_NOTES-r26p.ru.md). Local test build.

## r26o candidate — OptoCat Warm

- Add B1 Warm with native header highlighting, mild compensated soft saturation, 35% colour mix and first-order ADAA. Default off; smooth switching, dry Bypass and saved in-session state on wake.
- Preserve the clean optical detector/envelope, K/Shift controls, +6 dB Gain cap, Duck Color and all r26n fixes. Verify harmonics/alias reduction, mono/stereo, native PCM routing, gain bounds and UI state in ARM/QEMU/UBSan.
- Local device-test build; publication held at the author's request. [English](docs/RELEASE_NOTES-r26o.md) · [Русский](docs/RELEASE_NOTES-r26o.ru.md).

## r26n candidate — OptoCat Bypass underline

- Draw the native amber one-pixel underline for enabled OptoCat Bypass; clear it on wake and restore it on repeated sleep. Preserve r26m graphics, meters, pad-colour and input fixes; no audio DSP changes.
- Verify underline geometry/colour/state in production-painter ARM/QEMU and UBSan tests. [English](docs/RELEASE_NOTES-r26n.md) · [Русский](docs/RELEASE_NOTES-r26n.ru.md). Physical MPC validation pending.

## r26m candidate — final OptoCat integration and sleep display

- Combine final OptoCat through `c025d04` with completed r26l pad-colour/Copy/Extract/Seq fixes, preserving K, startup restore, artwork, meters and Shift controls.
- Clear displayed input/output/GR bars and numeric GR during Bypass; hide awake grain behind the sleeping cat. Restore on wake; preserve sprites, breathing, audio DSP/fades and the +6 dB Gain ceiling.
- Reproduce both old bugs and verify the production painter/compositor in ARM/QEMU and UBSan. Combined hardware/native-main validation remains pending. [English](docs/RELEASE_NOTES-r26m.md) · [Русский](docs/RELEASE_NOTES-r26m.ru.md).

## r26l candidate — sample colours and sequence selection

- Add assigned Sample pad colours and playback to Duck/OptoCat, preserving native Mute/Erase and other control layouts.
- Carry source Pad Colour through ordinary and batch Copy/Extract, with destination Undo, captured Redo and rollback. Keep source/occupied hues across Batch banks and retain destination/overwrite service indicators.
- Suppress playback flashes for muted instruments after leaving Mute. Keep the playing Seq selector steadily bright in its assigned hue; dim other occupied selectors and blink queued switches.
- Preserve K, bounded OptoCat gain/Shift controls/artwork and r26k startup gating. Waveform Mute visibility, first-pass Chop and drawing tint remain separate work. [English](docs/RELEASE_NOTES-r26l.md) · [Русский](docs/RELEASE_NOTES-r26l.ru.md). Local candidate; hardware trial and publication pending.

## r26k candidate — wait for native dialogs before startup project restore

- Prevent autoload from starting a second project while a native modal is open, including startup missing-sample/error dialogs that permit other component input. Restart the quiet interval after dialogs close; log the selected XPJ path.
- Reproduce the old ordering in ARM tests and the full native VM; verify deferred restore, the valid B fixture and a complete Mod Off copy with available sample data. Other reported load errors and recovery-copy naming remain separate investigation items.
- Preserve the device-confirmed K implementation and all r26j OptoCat changes. [English](docs/RELEASE_NOTES-r26k.md) · [Русский](docs/RELEASE_NOTES-r26k.ru.md). Hardware trial pending; not published.

## r26j candidate — OptoCat Shift controls and bounded gain

- Add Shift+K1 Attack (1–100 ms), Shift+K2 Release (100–4000 ms) and Shift+K3 Knee (0–12 dB), with names/values and per-event Shift capture. Preserve the default optical response.
- Connect Attack to paw strike speed and Release to its return, including timing changes during a gesture. Preserve the artwork, eyes, sleep and no repeated hits on steady input.
- Reduce Opto Gain from a maximum of +18 dB to +6 dB. Actual native Duck In Boost=100% measures +6.02060 dB; Opto's master adapter measures +5.99996 dB. Verify the boost ceiling during parameter changes.
- Preserve the device-confirmed r26i native K/takeover path. ARM/QEMU/UBSan and the full preload constructor pass; the new controls await a device trial. [English](docs/RELEASE_NOTES-r26j.md) · [Русский](docs/RELEASE_NOTES-r26j.ru.md).

## r26i — restore native knob delivery and takeover (issue #2)

- Forward unclaimed native K1–K3 on the original input thread, retaining FIFO, in-flight action and naming boundaries. Custom controls keep UI routing.
- Remove r26h's formula hook from the production library; audit the original RotaryFixedConnection and TrimParameter instructions without modifying them.
- Preserve OptoCat and prior mod features; expand bounded RAM input diagnostics. ARM/QEMU/UBSan cover original-thread delivery, action boundaries and unchanged takeover code. The author confirmed the K parameter fix on a physical MPC Sample after testing r26i on 2 November 2026. This confirms the reported hardware fix; it does not establish coverage of every control combination. [English](docs/RELEASE_NOTES-r26i.md) · [Русский](docs/RELEASE_NOTES-r26i.ru.md).

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

# AM Mod r26s — OptoCat, colours and installation fixes

[Русский](RELEASE_NOTES-r26s.ru.md)

This release brings together the fixes since the r26d download.

## Fixed

- K1–K3 no longer stall short of their end stops in Scaled mode (issue #2).
  Native input-thread delivery and the original takeover calculation are retained.
  The author confirmed this fix on an MPC Sample.
- Startup restore waits until native project/sample error dialogs close before
  opening the selected XPJ. Other reported project/Mod Off errors remain under investigation.
- OptoCat rebinds after New Project. Old callbacks/destructors cannot disconnect
  a replacement master. Bypass is shared with Duck; opening Shift+Pad5 shows the
  enabled type, or bypassed Duck when both are off. Browsing Type does not wake it.
- EQ no longer covers the native volume/pan strips beside the spectrum.
- Assigned pad hues remain in Duck/OptoCat, Sample playback and workflow views:
  ordinary Copy, sample browsing/search, Batch Copy/Extract, Stereo→Mono and
  resampling. Selected targets retain white cues and overwrite warnings remain red.
  Copy/Extract transfer source colours, including Undo/Redo and failure rollback.
- Muted instruments no longer flash playback colours outside the Mute overlay.
  The reported missing muted waveform and first-pass Chop light are separate open cases.

## Added and refined

- **OptoCat**, an experimental optical-style compressor beside native Duck.
  K1 Threshold −48…0 dB, K2 Ratio 1…8:1, K3 Gain **−6…+6 dB**, default Gain0.
  Shift+K1 Attack 1…100 ms, Shift+K2 Release 100…4000 ms, Shift+K3 Knee 0…12 dB.
- **B1 Warm** adds compensated mild saturation, default off. B2 Type chooses the
  algorithm; B3 Bypass switches processing off/on. Maximum makeup Gain remains +6 dB.
- The **bow represents dry input**; the **paw follows the same actual GR snapshot
  as the red meter**. Sustained compression holds contact; recovery returns the paw.
  Gain/Warm or loud input alone cannot trigger a strike. Sleep has an empty meter,
  black background and native Bypass underline. Shift badges show alternate controls.
- Seq pads keep their assigned hue and background glow: selected once per bar at
  Stop, playing once per beat, queued twice per beat until handoff. Other occupied
  pads retain the same brightness when Play starts.
- While holding **Sound** in Steps, the selected pad alternates white and its
  assigned hue, so the cursor no longer permanently hides its colour.
- **Experimental FAT32/MS-DOS (FAT32)** startup, alongside exFAT, with installer
  **1.0.2**. Use this complete package; old r26d packages still require exFAT.
  No reformatting is needed for an existing FAT32 card. Keep projects and samples.
  Use an ASCII card volume name, such as MPC; FAT32 limits individual files to
  4 GiB minus 1 byte. Physical FAT32 validation remains pending.

## Checks and remaining limits

ARM/QEMU normal and UBSan checks cover the real pinned native Colour factory and
process callbacks, DSP/gain/Warm, master replacement, project restore, Copy/Extract,
RGB ownership, EQ geometry and the stock resample adapter. All 50 preload callbacks
install before native main. Full native VM runs previously covered startup modal
ordering and project-copy fixtures; they are not a full combined r26s hardware test.
Installer tests: 16 passed, one macOS-only test skipped on Windows.

OptoCat Type/settings/Warm remain session-only. Native Bypass follows project state.
Startup picks the newest saved XPJ in SD Projects or one level below, rather than
the last manually opened project. Exact user project-error scenarios, ordinary
Copy navigation, muted waveform, first-pass Chop and the newest visual changes
need their remaining physical checks. See [known issues](KNOWN_ISSUES.md).

Thanks to **Leusha** for extensive testing, detailed reports and artwork ideas,
and to everyone who helped reproduce the filesystem and knob problems.

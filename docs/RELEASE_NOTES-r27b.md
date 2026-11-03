# r27b — prepared test release

Prepared locally on 3 November 2026. Not published; the public release remains r26s.
**Back up samples and projects before installing.** Firmware: MPC Sample 1.3.0.12.
exFAT is supported; **FAT32 is experimental**. No formatting is needed to update
an existing compatible card: copy the complete `autorun` folder to its root.
Keep existing samples/projects and unrelated folders. Replace the whole package,
not individual files from different revisions.

## Changes since public r26s

- Splash version/repository text is reduced to 8 pixels high and moved near the bottom edge.

- Five-page Mod Settings: Standard/AM Mod, waveform/spectrum tint, service/user
  palette swaps, working-pad cursor timing and About last. B2/B3 wrap; B1 returns.
  Restore recommended ends each editable section and requires B3 confirmation;
  About has no reset or editable controls. Five user colour slots remain.
- Standard keeps 16 Levels Piano/Scale layouts; custom Pad/Seq Colour remains
  disabled in Standard. Empty unassigned targets stay off, selected free targets
  are green and occupied targets red; batch sources and working cursors pulse.
- Note Repeat owns B1–B3 while open and no longer lets a queued hidden paint
  reopen the compressor Type overlay after release.
- Display preferences and OptoCat Type/controls/Warm are stored with the project.
  Native Duck and shared Bypass remain native. Failed loads preserve current
  state; legacy projects retain display preferences and reset OptoCat controls.
- Startup selects the SD XPJ during the first complete native Load; the late
  duplicate autoload is disabled. Selection still uses the newest saved XPJ,
  not a separate last-opened-project pointer.
- Normal shutdown saves a full recovery copy. Original identity is retained;
  recovered Save requires a separate first confirmation. Native Temp-Project
  autosave and the recovery sample directory are preserved. Mod Off is removed.
- Steps: SEQ RECORD WRITE and SAMPLE RECORD TOGGLE are mutually exclusive.
  Physical ERASE deletes in the browser with native confirmation.
- DRM splash is explicitly presented, releases master and yields to the native
  frame. The r27a device trial confirmed visibility after earlier black screens.
- New in r27b: splash version/repository address and read-only About (5/5),
  driven by one version header; splash duration is logged without adding a delay.

Retains the confirmed K end-stop fix and integrated OptoCat/Warm, Gain −6…+6 dB,
Shift Attack/Release/Knee, shared Bypass, real-GR paw/red bar, EQ side strips,
native filter/envelope curves, workflow colours and Seq tempo pulses.

## Validation and limits

ARM/QEMU normal and UBSan checks cover About read-only controls and five-tab
wrapping, project recovery/settings, native filters and retained workflows.
The r27b library passed the full preload constructor checks. Native VM display
and menu results are recorded in the accompanying release verification files.
These are distinct from physical device confirmation. The author confirmed
the K fix and r27a splash; r27b metadata still needs a device check.

One r27a device journal measured 17 seconds from splash readiness to the app
start request (copy/hash and sandbox checks included). There is no artificial
minimum splash time. This release does not claim a boot-speed improvement.

L01–L04 project lifecycle checks are not all closed; L06 first-loop Chop light,
L11 muted waveform and native Copy Shift+Bank still need their exact device
scenarios. FAT32 still needs physical validation. See [known issues](KNOWN_ISSUES.md)
and [readiness](RELEASE_READINESS-r27b.md). This is a test release, not a claim
that every reported issue is fixed.

Thanks to **Leusha** and all testers for detailed reports and reproduction steps.

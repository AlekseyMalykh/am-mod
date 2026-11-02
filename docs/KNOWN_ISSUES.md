# Known issues and testing status

[Русский](KNOWN_ISSUES.ru.md) · [Report / discuss](FEEDBACK.md)

Updated **2 November 2026**. These are reports under investigation, not all
independently reproduced bugs. Numbers L01–L21 preserve the supplied tester
summary; L22 is an additional report from the author. Revision and exact steps
are still needed for some reports. Related items remain separate test cases.

**Current release: r26s.** Source and ZIP include the confirmed K fix,
experimental FAT32, OptoCat/Warm, shared Bypass/master rebinding, startup modal
gating, Copy/Extract colour history, workflow hues, Seq pulses and Sound cursor.
EQ retains its native volume/pan side strips; OptoCat Gain is −6…+6 dB.
[Changes and verification scope](RELEASE_NOTES-r26s.md).

The author confirmed K on an MPC. ARM/QEMU/UBSan cover the specified native
factory/audio/renderer/workflow boundaries; the newest combined GUI scenarios
and physical FAT32 still need their device checks. A previous full native VM
run verified one startup-modal error and project-copy fixture; it does not close
every project report below. Ordinary manual External Save/Load was reported to work.

## Reports and remaining checks

| ID | Priority | Report / next check |
| --- | --- | --- |
| L01 | P1 | Complete-project loading was fixed in r26b–r26d. Recheck the full startup/Mod Off cycle: project B must retain its own sequences, notes, samples and mixer state over project A or an empty project. This is follow-up coverage, not a confirmed new regression. |
| L02 | P1 | Several `Could not load project` messages after Mod Off, stock project loading and restart. Exact sequence is uncertain; identify the XPJ chosen at startup and its data folder. |
| L03 | P1 | `Could not load sample` during Mod Off. Check the exit-copy sample paths, files and handoff to stock. |
| L04 | P1 | `Mod Exit Last` appears as the project name in ordinary Save. The exit copy is intentional; check project identity and the destination of a later Save. |
| L05 | P2 | Delete appears unavailable in Project Load. Physical **Erase** in Open Project is already implemented; check the affected browser/search state and selected file/folder. |
| L06 | P2 | A Chop on the first beat does not light on the first Play pass, but lights after looping. Reproduce the first-pass and restart cases. |
| L08 | P2 | Implemented: assigned hues across ordinary Copy/browser and custom Batch/Mono/Resample/Extract; target/overwrite cues preserved. Retest the reported bank/Paste/Cancel sequence on the MPC. |
| L09 | P2 | Implemented source-colour inheritance for Copy/Extract, captured Undo/Redo and rollback. Clearing preserves the pad colour by design. Verify device Save/Load and used/empty targets. |
| L11 | P2 | Muted pads look empty because the waveform is hidden. Verify sample presence independently of the load errors; consider a visible dim waveform and clear Mute state. |
| L13 | P2 | Shift+Pad Bank does not go to the previous bank during Copy. Check native reverse navigation in that context. A bank picker is a separate suggestion. |
| L16 | P2 | r26s implements the agreed Seq pulses: selected once per bar at Stop, playing once per beat, queued twice per beat until handoff, with visible background and stable other-pad brightness. Verify timing/readability and queue/cancel on the MPC before closing. |
| L22 | P2 | Implemented assigned colours in Duck/OptoCat, including playback and banks; Mute/Erase priorities retained. Retest both types, Bypass, selection and exit on the MPC. |

Project lifecycle (L01–L04) is the first investigation priority, followed by
Chop and Copy/Extract/compressor colours. Ordinary manual Save/Load was reported
to work with External projects; automatic restoration and Mod Off need separate checks.

## Fixed, implemented or awaiting a specific retest

| ID | Status |
| --- | --- |
| L07 | Sample playback retaining Pad Colour was fixed in r26b–r26d and confirmed by the author. Retest the tester's case on a current build; Copy and compressor contexts remain L08/L22. |
| L10 | Green was made distinct from Cyan in r26b–r26d; the tester noticed the change after installing r26d. |
| L12 | **K1–K3 end-stop stalls fixed in r26i**, confirmed on hardware by the author. [Issue #2 is closed](https://github.com/AlekseyMalykh/am-mod/issues/2). Included in the r26s ZIP. |
| L20 | Spectrum on Filter and Filter Env, in addition to EQ, is implemented in r26d. Native curves are preserved. |
| L21a | Pad Colour without Shift is implemented: **Sample → B1 Mix → K2**; Shift+K2 is Kit Volume, K1 is Volume. |

## Questions and feature requests

- **L14:** clearer instructions for Piano/Scale in 16 Levels Tune and Sample Lights.
- **L15:** another entry point for Key/Scale in Input Config; currently a project setting.
- **L17:** investigate the visibly coarser low-frequency spectrum resolution before calling it a bug.
- **L18:** brighter spectrum without obscuring native filter/envelope curves.
- **L19:** waveform/spectrum colours derived from Pad Colour, retaining readable contrast.
- **L21b:** a dedicated RGB editor.
- **L21c:** interchangeable compressor characters. OptoCat already exists; B2 currently selects **Duck/OptoCat algorithms**, not an independent artwork skin.
- **L21d:** offer to help with artwork; no asset task has been agreed yet.

These are suggestions, not scheduled commitments. OptoCat Attack/Release/Knee,
GR-driven paw, Warm and the gain limits are included in r26s; see its verification scope.

## Current limitations

- Experimental FAT32 is in r26s and packages from r26e onward; physical FAT32
  confirmation is still pending. r26d packages require exFAT.
- OptoCat type/settings currently live in RAM and are not saved in projects.
- Startup chooses the newest saved XPJ in SD Projects or one level below,
  rather than keeping a history of manually opened projects.

See [r26d](RELEASE_NOTES-r26d.md), [r26i](RELEASE_NOTES-r26i.md) and
[installation/control documentation](../README.md) for the scope of existing fixes.

# Known issues and testing status

[Русский](KNOWN_ISSUES.ru.md) · [Report / discuss](FEEDBACK.md)

Updated **2 November 2026**. These are reports under investigation, not all
independently reproduced bugs. Numbers L01–L21 preserve the supplied tester
summary; L22 is an additional report from the author. Revision and exact steps
are still needed for some reports. Related items remain separate test cases.

**Version matters:** public source `main` includes **r26i**, with the K knob
fix confirmed on an MPC. The latest downloadable ZIP is still **r26d**, which
does not include that fix or experimental FAT32. Local r26j compressor changes
have not been published or confirmed on hardware.

## Open reports

| ID | Priority | Report / next check |
| --- | --- | --- |
| L01 | P1 | Complete-project loading was fixed in r26b–r26d. Recheck the full startup/Mod Off cycle: project B must retain its own sequences, notes, samples and mixer state over project A or an empty project. This is follow-up coverage, not a confirmed new regression. |
| L02 | P1 | Several `Could not load project` messages after Mod Off, stock project loading and restart. Exact sequence is uncertain; identify the XPJ chosen at startup and its data folder. |
| L03 | P1 | `Could not load sample` during Mod Off. Check the exit-copy sample paths, files and handoff to stock. |
| L04 | P1 | `Mod Exit Last` appears as the project name in ordinary Save. The exit copy is intentional; check project identity and the destination of a later Save. |
| L05 | P2 | Delete appears unavailable in Project Load. Physical **Erase** in Open Project is already implemented; check the affected browser/search state and selected file/folder. |
| L06 | P2 | A Chop on the first beat does not light on the first Play pass, but lights after looping. Reproduce the first-pass and restart cases. |
| L08 | P2 | Pad Colour resets when changing banks **inside Copy/Paste**, rather than during ordinary bank changes. Distinguish temporary LED changes from altered stored colour assignments. |
| L09 | P2 | Extract Chop does not inherit the source colour; cleared destinations can retain an old colour. Decide the intended inheritance/reset rule, then check new/used/cleared targets and Undo. |
| L11 | P2 | Muted pads look empty because the waveform is hidden. Verify sample presence independently of the load errors; consider a visible dim waveform and clear Mute state. |
| L13 | P2 | Shift+Pad Bank does not go to the previous bank during Copy. Check native reverse navigation in that context. A bank picker is a separate suggestion. |
| L16 | P2 | Sequence Colors sometimes blink while playback is stopped. Check selected, playing and queued sequences before treating this solely as a visual preference. |
| L22 | P2 | **Assigned pad colours do not appear on the compressor screen.** Check both Duck and OptoCat, bank changes, playback, pad selection, Mute/Bypass and screen exit. A loss of saved colour data has not been established. |

Project lifecycle (L01–L04) is the first investigation priority, followed by
Chop and Copy/Extract/compressor colours. Ordinary manual Save/Load was reported
to work with External projects; automatic restoration and Mod Off need separate checks.

## Fixed, implemented or awaiting a specific retest

| ID | Status |
| --- | --- |
| L07 | Sample playback retaining Pad Colour was fixed in r26b–r26d and confirmed by the author. Retest the tester's case on a current build; Copy and compressor contexts remain L08/L22. |
| L10 | Green was made distinct from Cyan in r26b–r26d; the tester noticed the change after installing r26d. |
| L12 | **K1–K3 end-stop stalls fixed in r26i**, confirmed on hardware by the author. [Issue #2 is closed](https://github.com/AlekseyMalykh/am-mod/issues/2). The r26d ZIP is older than this fix. |
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

These are suggestions, not scheduled commitments. Local Attack/Release/Knee,
paw timing and gain-limit work must not be described as a publicly released fix.

## Current limitations

- Experimental FAT32 is in the source version from r26e onward; physical FAT32
  confirmation is still pending. r26d packages require exFAT.
- OptoCat type/settings currently live in RAM and are not saved in projects.
- Startup chooses the newest saved XPJ in SD Projects or one level below,
  rather than keeping a history of manually opened projects.

See [r26d](RELEASE_NOTES-r26d.md), [r26i](RELEASE_NOTES-r26i.md) and
[installation/control documentation](../README.md) for the scope of existing fixes.

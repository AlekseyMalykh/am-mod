# r27c — test candidate

Local candidate, not published. The public release remains r26s.
**Back up samples and projects before installing.**
MPC Sample 1.3.0.12; exFAT supported, FAT32 experimental.
Replace the complete autorun folder; formatting is unnecessary.

## Changes since r27b

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

Preserves OptoCat/Warm, the Scaled/K fix, project settings, a single full
startup Load, small version/repository text at the bottom of the splash,
filter curves/envelopes and EQ side meters.

Factory reset covers Sample, EQ, OptoCat, Key/Scale, Lights, Chords and
Steps settings. Object selection (Sound/Bank), menus and unknown controls
are unchanged. Native Duck/other FX do not yet have factory-default tables;
the gesture does nothing there. Master Volume is unaffected.

## Validation

ARM and UBSan regressions cover autosave, colour priorities, all four cursor
speeds, independent normal/Shift reset, input ordering and preserved Scaled.
The full VM reproduced the original New Project autosave failure and then
created a readable empty XPJ with the fix. Physical MPC testing is pending.

Test New Project for over a minute, End reset to100, Shift Zoom reset to0,
held-bank pad selection, and configurable selection in library/Copy/Batch/
Mono/Render. Red/green destinations and unselected empty-off pads must remain.

[Changes since r26s](RELEASE_NOTES-r27b.md) · [Open reports](KNOWN_ISSUES.md) ·
[Settings](MOD_SETTINGS.md).

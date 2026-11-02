# r26l candidate — Pad Colour, Copy/Extract and sequence lights

[Русский](RELEASE_NOTES-r26l.ru.md)

Local candidate over r26k. Not published or confirmed on an MPC.

Includes the neighbouring OptoCat chat's committed changes through `1b06536`:
new seated animation, input/output and GR meters, and native Shift badges.
That chat owns those changes; this work adds only a visibility snapshot for
the pad palette. Its worktree and card installation remain independent. The
earlier archive without these changes is retained separately, not offered for
installation.

- **Compressor (L22):** Duck and OptoCat share the assigned colours of the
  current Sample bank, including playback activity. Type and Bypass do not
  change assignments. Native Mute/Erase and other control layouts take priority.
  UI visibility is published as a numeric snapshot; RGB never reads Components.
- **Copy/Extract (L09):** ordinary Copy and Extract inherit the source Pad
  Colour. Native command history restores the destination colours on Undo and
  the captured source colour on Redo. Failed perform/undo does not apply success
  metadata. Batch commands snapshot all sources before writing, including
  overlapping cycles, and restore colours during rollback.
- **Batch Copy/Extract (part of L08):** source/occupied pads retain their hues
  across banks. Destination selection stays white and overwrite warnings stay
  red with brightness changes. Ordinary native Copy LED behaviour still needs
  a separate retest; its service palette is not changed here.
- **Mute:** muted instruments no longer receive playback flashes after leaving
  the Mute overlay. This reads the instrument mute property, not transient LED
  colours. Native red/yellow Mute indicators are retained. The hidden-waveform
  report L11 remains separate and is not closed by this change.
- **Seq (L16):** the playing sequence selector stays steadily bright in its
  assigned hue; other occupied slots are dimmer and queued switches blink.
  No white replacement is introduced. Stopped selection keeps native flashing.

Preserves r26i native K delivery, r26j OptoCat Shift controls, the +6 dB Gain
ceiling, paw timing/artwork and r26k native-modal startup gating.

## Validation and remaining work

ARM/QEMU and UBSan cover production RGB, screen/bank transitions, Mute/Erase
priority, steady playing/queued sequence states, real native multi-target Copy
perform/undo across banks, native Extract constructor/destructor and colour
history, failed operations, command storage reuse and batch rollback. Extraction
PCM/file persistence remain fixture boundaries in the history test. Separate
checks cover K, colour project persistence, EQ Reset history and OptoCat DSP,
controls and animation. Physical MPC testing and the user's complete Save/Load
cycle are still required before claiming a device-confirmed fix.

The pre-merge colour build was also exercised in a full native VM: Blue stayed
assigned on Sample, Duck, OptoCat and Bypass; held Mute showed the native muted
red and release restored Blue. The muted waveform remained visible grey and
Unmute restored it, so L11 was not reproduced on that synthetic sample. This
is evidence for the pre-merge palette build, not a full post-merge native VM
run. The combined build has separate ARM/UBSan and constructor validation.

Still open: first-pass Chop lighting (L06), ordinary Copy LEDs/Cancel/Paste
(L08), visible grey muted waveform (L11), native reverse Copy-bank navigation
(L13), spectrum brightness/tint and waveform tint (L18/L19), and the optional
RGB editor (L21b). Spectrum tint is a small painter change; waveform tint needs
a more careful native drawing scope that preserves Mute grey, markers and
filter/envelope curves. Neither drawing colour changes in r26l. Clearing a
sample still retains its pad's assigned colour; Extract explicitly replaces it
with the source colour.

Leusha's detailed testing is recorded in the [triage](TESTER_FEEDBACK_LEUSHA.ru.md).
The thank-you reply has not been sent.

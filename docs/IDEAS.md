# Ideas and future development

Requests **not included in r27e**, without scheduled delivery dates. Discuss ideas
in [GitHub Discussions](https://github.com/AlekseyMalykh/am-mod/discussions).
Current defects and checks are in [Known issues](KNOWN_ISSUES.md).

## C01 — live chord and arpeggiator changes

Change notes, chord voicing/inversion and arpeggiator parameters during playing,
including held pads, without requiring release/retrigger. Decide whether changes
apply immediately or on the next beat/arp step, with correct Note Off ownership
and no stuck notes.

Some arpeggiator settings are already read while running. Held chord pitches are
currently captured at press time; changing chord quality does not rebuild them.
The request extends that behaviour; it is not a newly implemented r27e feature.
Include sequence recording, shared notes, Repeat, latch, tempo changes and exit
in its eventual design and tests.

## C02 — expression from current pad force

Control chord/arpeggiator dynamics from current playing force, with adjustable
sensitivity/response and clear behaviour for multiple held pads.

New-hit velocity is already passed through. This request also considers changing
held-chord expression and subsequent arp notes during a hold. Before promising
continuous pressure/aftertouch, verify which data MPC Sample actually reports
after the initial strike and how Full Level/16 Levels affect it. Required
hardware support is not confirmed yet.

## Other requests

- **Synth:** deferred by the author; not being implemented now.
- **Stem export:** separate sounds/tracks, not implemented.
- **Mixer and effects:** design level/pan/inserts/sends/routing; no shortcut has
  been chosen. This remains separate work.
- Independent RGB editor, compressor artwork choices, spectrum brightness/detail
  and a Key/Scale shortcut: see the original request IDs in Known Issues.

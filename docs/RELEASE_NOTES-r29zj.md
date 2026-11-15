# r29zj

- New (Am MOD): Mod Settings > Sample > "Pad colour from": Pad (default, the pad's own colour) or Track. With Track, the pads in the Sample window, the waveform tint and the spectrum tint use the colour of the Mixer track the pad is assigned to (Shift+K1 Output). Pads sent to Master Direct use the Master colour. In Track mode Pad Colour (Sample Mix K2) cannot be edited and shows "Track". Saved on the card (`.am-mod-pad-colour`), applies to all projects; greyed in Stock and when Pad lighting is not Am MOD.
- Includes r29zg (Save fix) and r29zf (resample diagnostics).

## r29zj
- "Pad colour from: Track" now applies on every screen that shows pad sounds: Step Sequencer (sound header, event list, pad lights, chop picker, playhead), the Sample browser and copy/move pad lights. Master Direct pads use the Master colour.

## r29zj
- Sample browser: the K3 Volume read-out no longer blinks or disappears while scrolling (kept 1.5 s after the last audio selection, read-out latched for 2 s).
- Diagnostics: OFFLINE_CALLER lines in the log for sequence-resample investigation.

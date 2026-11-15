# r29zp

- New (Am MOD): Mod Settings > Sample > "Pad colour from": Pad (default, the pad's own colour) or Track. With Track, the pads in the Sample window, the waveform tint and the spectrum tint use the colour of the Mixer track the pad is assigned to (Shift+K1 Output). Pads sent to Master Direct use the Master colour. In Track mode Pad Colour (Sample Mix K2) cannot be edited and shows "Track". Saved on the card (`.am-mod-pad-colour`), applies to all projects; greyed in Stock and when Pad lighting is not Am MOD.
- Includes r29zg (Save fix) and r29zf (resample diagnostics).

## r29zp
- "Pad colour from: Track" now applies on every screen that shows pad sounds: Step Sequencer (sound header, event list, pad lights, chop picker, playhead), the Sample browser and copy/move pad lights. Master Direct pads use the Master colour.

## r29zp
- Sample browser: the K3 Volume read-out no longer blinks or disappears while scrolling (kept 1.5 s after the last audio selection, read-out latched for 2 s).
- Diagnostics: OFFLINE_CALLER lines in the log for sequence-resample investigation.

## r29zp
- Sequence resample now goes through the Mixer: the hook missed the sequence render (source 0, native caller 18b5570), so tracks, faders, sends and FX were skipped and a pad at 0 was still heard.

## r29zp
- Sequence resample destination dialog (Shift+pad11): pads use the pad/track colours and a white blinking cursor instead of stock light blue and yellow.

## r29zp
- Sequence resample: the Mixer render learns the program type from the UI when no pad has played yet (a first resample after start could be silent).
- The stock Shift+pad11 destination dialog is recognised from the native gesture; K3 Volume read-out is repainted while a sample is selected.

## r29zp
- Sequence window: Shift+pad11 opens the mod resample dialog straight on the destination page (no sequence/sample question), in both Stock and Am MOD: Am MOD pad colours, replace confirmation for an occupied pad, then the stock render (through the Mixer in Am MOD, through FX in Stock).

## r29zp
- Resample dialogs: the panel bank letters (A-H) follow the dialog bank. Pad Bank steps forward, Shift + Pad Bank steps back; a resample started from the Sequence window opens on the current bank.

## r29zp
- Mixer: Shift+Erase is consumed by the Mixer and no longer starts the stock copy-from-pads gesture.

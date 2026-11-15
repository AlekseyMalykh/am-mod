# r29zw

## r29zw
- Tune event editor (16 Levels, L events): **Shift+K1** switches the sound layout (Monochrome / Piano / Scale / Degrees), the same setting as Sample > Tune > Play > Scales. In Degrees the pads play key notes by degree and K3 / +/- set Transpose16.
- While the sequence plays in the editor, the replaced note sounds at the event position, and the notes played at other beats flash on the pads.

## r29zw
- Sequence resample from the Sequence window confirmed working (r29zt).
- Sample browser: K3 Volume is redrawn every 100 ms while a sound is selected, so the stock list repaint during scrolling no longer wipes it.
- Diagnostics: REC_COLOURS (final Sample Record pad colours).

## r29zw
- Resample from the Sequence window now renders the sequence (the dialog used to open as "sample", so the selected pad sound was rendered instead).
- Sample browser: the K3 Volume read-out is hidden at once when a folder is selected after a sound (the hold-over only bridges list rebuilds while scrolling).
- Sample Record screen: pad lights use Am MOD colours even when the bank cannot be read from the screen (last known bank), and the used/free pad mask is refreshed continuously. Diagnostics: RESAMPLE DIALOG_OPEN, REC_PADS.

- New (Am MOD): Mod Settings > Sample > "Pad colour from": Pad (default, the pad's own colour) or Track. With Track, the pads in the Sample window, the waveform tint and the spectrum tint use the colour of the Mixer track the pad is assigned to (Shift+K1 Output). Pads sent to Master Direct use the Master colour. In Track mode Pad Colour (Sample Mix K2) cannot be edited and shows "Track". Saved on the card (`.am-mod-pad-colour`), applies to all projects; greyed in Stock and when Pad lighting is not Am MOD.
- Includes r29zg (Save fix) and r29zf (resample diagnostics).

## r29zr
- "Pad colour from: Track" now applies on every screen that shows pad sounds: Step Sequencer (sound header, event list, pad lights, chop picker, playhead), the Sample browser and copy/move pad lights. Master Direct pads use the Master colour.

## r29zr
- Sample browser: the K3 Volume read-out no longer blinks or disappears while scrolling (kept 1.5 s after the last audio selection, read-out latched for 2 s).
- Diagnostics: OFFLINE_CALLER lines in the log for sequence-resample investigation.

## r29zr
- Sequence resample now goes through the Mixer: the hook missed the sequence render (source 0, native caller 18b5570), so tracks, faders, sends and FX were skipped and a pad at 0 was still heard.

## r29zr
- Sequence resample destination dialog (Shift+pad11): pads use the pad/track colours and a white blinking cursor instead of stock light blue and yellow.

## r29zr
- Sequence resample: the Mixer render learns the program type from the UI when no pad has played yet (a first resample after start could be silent).
- The stock Shift+pad11 destination dialog is recognised from the native gesture; K3 Volume read-out is repainted while a sample is selected.

## r29zr
- Sequence window: Shift+pad11 opens the mod resample dialog straight on the destination page (no sequence/sample question), in both Stock and Am MOD: Am MOD pad colours, replace confirmation for an occupied pad, then the stock render (through the Mixer in Am MOD, through FX in Stock).

## r29zr
- Resample dialogs: the panel bank letters (A-H) follow the dialog bank. Pad Bank steps forward, Shift + Pad Bank steps back; a resample started from the Sequence window opens on the current bank.

## r29zr
- Mixer: Shift+Erase is consumed by the Mixer and no longer starts the stock copy-from-pads gesture.

## r29zr
- Resample dialogs: holding Pad Bank lights pads 1-8 red and they switch the bank (a tap still steps forward, Shift+tap back).
- Sample browser: the K3 Volume read-out is no longer lost when the list is briefly reported hidden while scrolling.
- Sample Record target screen: Am MOD pad colours and the pad cursor instead of stock yellow.

## r29zr
- Sequence window: pad colours are no longer shown during playback (the Sample Record detection mistook it for that screen).
- Resample dialogs: releasing Pad Bank after a hold (300 ms) no longer steps the bank; only a tap does.
- Sample browser: the K3 Volume caption is cleared when it expires (leaving an audio folder).

## r29zw
- L-event editor (Tune): K1 Transpose16, Shift+K1 layout, K3 type.
- Original note is not muted while it stays selected.
- Multi-note choice only for Poly samples; Mono keeps one note.

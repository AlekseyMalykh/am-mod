# r29z (test build after r29k)

**Am MOD: Sample resample (single pad) now goes through the Mixer.**

- The private-note resample render uses the same offline Mixer path as Song stems: Track fader/Mute/Solo, Track FX, Sends, Master FX and Pad Link apply to the result. The stock FX Select is ignored (as in r29k).
- No stem files are written; the processed Master is what the stock writer saves into the new sample.
- Stock mode is unchanged (no Mixer, stock FX Select applies).
- Seq resample is not changed in this build.

[Русский](RELEASE_NOTES-r29z.ru.md)

r29z: the new sample is taken from the Mixer's Master file written by the stock writer (not from the raw track stem), so Track/Master effects, Mute and Sends are in the result.

## r29z
- Resample from the Sample screen: the "Seq / Sample" question is gone; the source is the selected pad, and the destination pad is chosen right away.
- Sequence resample in Am MOD: the render now goes through the Mixer (Tracks, Sends, Track FX, Master FX), like a Song export.
- Known issues: USB transfer mode is unavailable while the mod runs; remove the `autorun` folder to restore stock.
- Recording route (Project -> Mod Settings -> Quick): "Record via" Input / Track / Master, "Record track" A-H, "Record FX" Dry / Wet. Track: Input FX, then the track's FX, fader and pan. Master: then the Master FX and Master fader. The monitor hears the same signal. One setting for Mic, USB and Rear (per-source choice later). The sample compressor is not part of the recording path yet.
- Song export in Stock no longer lists the Stems entries (stock firmware has no stem export); they are shown only in Am MOD.
- USB transfer mode does not work while the mod runs (see README); the native menu item stays but cannot present the card.

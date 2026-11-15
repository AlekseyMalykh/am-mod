# r29u (test build after r29k)

**Am MOD: Sample resample (single pad) now goes through the Mixer.**

- The private-note resample render uses the same offline Mixer path as Song stems: Track fader/Mute/Solo, Track FX, Sends, Master FX and Pad Link apply to the result. The stock FX Select is ignored (as in r29k).
- No stem files are written; the processed Master is what the stock writer saves into the new sample.
- Stock mode is unchanged (no Mixer, stock FX Select applies).
- Seq resample is not changed in this build.

[Русский](RELEASE_NOTES-r29u.ru.md)

r29u: the new sample is taken from the Mixer's Master file written by the stock writer (not from the raw track stem), so Track/Master effects, Mute and Sends are in the result.

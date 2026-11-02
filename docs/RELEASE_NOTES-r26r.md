# r26r candidate — compression-driven paw and Sound cursor

[Русский](RELEASE_NOTES-r26r.ru.md)

The bow represents dry input level; the paw represents actual compression.
OptoCat now gives the paw, red GR bar and numeric GR the same DSP snapshot.
More reduction extends the paw; sustained reduction holds contact and recovery
returns the paw. Attack/Release already shape that audio envelope: no second
visual envelope, threshold-only trigger or synthetic repeated strike is used.
Loud input with no GR, makeup Gain and Warm alone cannot move the paw.
The animation uses a bounded nonlinear reach mapping (1-exp(-GR/3)); the red
bar retains its calibrated 0–60 dB scale. DSP, gain cap, artwork and sleep remain.

While B1 Sound is held in Steps, the selected pad alternates white for 240 ms
and its assigned colour for 400 ms. Other pad colours and selection are unchanged.
A selected empty slot keeps a white phase and shows its assigned colour if any,
otherwise its empty indication. The ordinary Steps cursor is unchanged.

ARM/QEMU and UBSan cover real DSP Attack/Release-to-paw timing, held GR, zero/invalid
GR, loud Ratio1/Gain/Warm input without compression, native Bypass/graphics,
all 13 colour choices/eight banks and empty Sound selections. r26q Seq pulses,
K input, Copy/Extract, shared Bypass and master lifecycle fixes remain.
Full native-main/physical MPC validation of these two visual changes is pending.
Local candidate, unpublished.

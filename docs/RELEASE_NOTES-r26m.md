# r26m candidate — final OptoCat and combined mod fixes

[Русский](RELEASE_NOTES-r26m.ru.md)

Combines the neighbouring chat's final OptoCat through `c025d04` with completed
r26l compressor/Batch pad hues, Copy/Extract colour Undo/Redo, muted-source
playback and sequence-selector fixes. Preserves native K delivery, startup
modal gating, seated animation, stereo/GR meters and Shift badges. Remaining
Leusha reports are still tracked in [r26l](RELEASE_NOTES-r26l.md).

Bypass previously put the cat to sleep while meter bars and awake grain remained.
Input/output/GR bars now clear immediately, numeric GR shows 0 dB, and the cat
sleeps on black. Dark meter tracks remain. Grain and activity return on wake.
Sprites, poses, breathing, actual signal telemetry, DSP, audio fades and the
+6 dB Gain ceiling are unchanged.

ARM/QEMU and UBSan reproduce both old bugs from `9f4d5a1` and verify real meter
pixels, GR before the next audio block, sustained bypass input, wake/repeated
sleep, opaque black outside the sleeping sprite throughout tested breathing
positions, and restored awake grain. DSP/animation, Shift, stereo/gain tests,
pad RGB and the full preload constructor with all 50 callbacks pass. Existing
K, Copy/Extract and project lifecycle proof hashes are checked during packaging.

The painter test uses native ARM Image/Colour/audio factories with adapted
Component/Graphics receivers. Full combined r26m native-app startup and physical
MPC validation remain pending. This is a local candidate, not a public release.

# r26n candidate — OptoCat Bypass underline

[Русский](RELEASE_NOTES-r26n.ru.md)

Enabled OptoCat Bypass now has the native amber, one-pixel underline with the
same single-option header margins. It clears on wake and returns on repeated
Bypass. The custom OptoCat header previously replaced the native header with
text only. ARM/QEMU/UBSan verify geometry, colour and state transitions alongside
the existing audio/animation checks; the full preload constructor passes.
Physical MPC validation is still pending.

Preserves all completed r26m fixes, including sleeping black background/empty
meters, final artwork, pad colours, Copy/Extract, Seq, K and startup restoration.
Audio DSP is unchanged. Duck and OptoCat are alternatives in one master slot,
with a 10 ms parallel crossfade between results of the same dry input, never
serial compression. OptoCat Bypass does not engage Duck. Native Color belongs
to Duck; additional Warm/Tape saturation for OptoCat is not implemented yet.

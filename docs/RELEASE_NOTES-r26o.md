# r26o candidate — OptoCat Warm

[Русский](RELEASE_NOTES-r26o.ru.md)

**B1 Warm** toggles mild saturation on the OptoCat screen, with an amber label
and underline when enabled. Default is off. B2 selects Duck/OptoCat; B3 Bypass
disables the compressor and Warm together, retaining the chosen Warm state on
wake. Compressor settings live in RAM and are not saved in projects yet.

Fixed character: cubic soft saturation, Drive1.5 with reciprocal drive
compensation, 35% coloured mix, first-order ADAA alias reduction and ~10 ms
switch/Bypass fades. Adds harmonics and softens high frequencies without pitch
modulation, noise or automatic makeup boost. The dry RMS detector and optical
gain-reduction envelope are unchanged; all K/Shift controls and +6 dB Gain cap
remain. Warm off retains the previous clean DSP. Duck's native Color is isolated.

ARM/QEMU/UBSan cover measured harmonics/DC/alias reduction, mono/stereo and
opposite polarity, block invariance, sample rates 8–192 kHz, boundaries, silence, smooth
switching, dry Bypass and the gain bound. Actual native master callbacks verify
B1 pairing/header state, PCM changes and Duck isolation. Warm measured
5.989/5.988/5.881/5.552 dB at Gain+6 for a 1 kHz sine at -60/-30/-12/-6 dBFS.
The clean response matches r26i; detector/GR are unchanged by Warm. Full preload
construction and K/pad-colour regressions pass. All completed r26n fixes remain.

Drive compensation is not a promise of identical perceived loudness for every
source. Physical MPC listening is pending: try OptoCat at Gain0 and compare B1
on the same loop. Local test build; publication held at the author's request.

ADAA reference: [Bilbao et al.](https://www.research.ed.ac.uk/files/34115216/bilbao_pdf.pdf).

# r26j candidate — OptoCat controls, paw timing and gain limit

[Русский](RELEASE_NOTES-r26j.ru.md)

On the compressor screen (**Shift+Pad 5 → B2 Type → OptoCat**):

| Knob | Normal | With Shift |
| --- | --- | --- |
| K1 | Threshold −48…0 dB | Attack 1…100 ms |
| K2 | Ratio 1…8:1 | Release 100…4000 ms |
| K3 | Gain −12…+6 dB | Knee 0…12 dB |

Shift changes the displayed names and values. Each queued movement retains
its Shift state; releasing Shift before dispatch cannot redirect an Attack
movement to Threshold. B2 Type and B3 Bypass retain their behaviour.

Attack controls the speed of the paw strike; Release controls the return.
The shortest attacks have a minimum visible animation duration. Timing edits
affect an ongoing gesture without restarting it. The bow follows the paw;
steady input does not generate repeated strikes. Gain still opens the eyes,
and Bypass puts the kitten to sleep. The approved artwork is unchanged.

Release is the optical memory tail; the fast recovery component is Release/20.
Defaults remain **10 ms Attack, 1200 ms Release, 6 dB Knee**. With these values,
the DSP retains the previous 10 ms attack, 60 ms fast recovery and 1200 ms tail.
Knee=0 provides a hard knee. Settings remain in RAM and are not saved in projects.

## Gain comparison

The actual stock Duck DSP, with Amount=0 and Color off, produces **+6.02060 dB**
at In Boost=100%. The production Opto adapter now measures **+5.99996 dB** at
Gain=+6 dB and Ratio=1, across input peaks of −60, −30, −12 and −6 dBFS.
Opto's previous +18 dB maximum has been reduced to **+6 dB**. This limits added
gain, rather than imposing a hard clip on output samples. Compression settings
still affect the final level; the two algorithms do not produce identical audio.

## Verification

- ARM/QEMU and UBSan: actual stock and Opto master gain comparison, stereo,
  bypass, parameter isolation, Attack/Release/Knee response and +6 dB boost
  ceiling during parameter changes.
- Default DSP output compared with the approved r26i implementation.
- Shift controls, names, bounds, queued modifier capture and queue separation.
- Paw Attack/Release timing, changes during recovery, steady-input behaviour,
  eye animation, sleep/wake and the real native image blit.
- All 22 input regressions and the full 50-callback preload constructor.
  The r26i native knob delivery/takeover implementation is unchanged.

Firmware **1.3.0.12**, exFAT or experimental FAT32. Replace the complete
`autorun/` folder, or use installer 1.0.2 with the r26j ZIP. Keep existing
projects and samples. This is a local device-test candidate; r26j has not
yet been confirmed on hardware or published as a GitHub release.

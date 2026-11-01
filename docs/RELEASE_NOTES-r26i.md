# r26i candidate — native knob input and takeover

[Русский](RELEASE_NOTES-r26i.ru.md) · [Issue #2](https://github.com/AlekseyMalykh/am-mod/issues/2) still needs hardware confirmation.

The device log from the unsuccessful r26h Start/End test includes exact 0/127
input on K1/K2 with only one or two queued events. It does not establish the
internal TrimParameter result or the precise cause of sustained device stalls.

- Forward ordinary K1–K3 through their original handler on the original input
  thread when no mod control owns them. Observing Sample/Seq no longer imposes
  the mod UI timer's 50ms batching and event merging on native parameters.
- Retain UI delivery for custom controls, naming and queued/in-flight action
  boundaries, including nested UI dispatch. Knob movements cannot overtake a
  pending page/Shift action.
- Remove r26h's endpoint formula hook from the production library. Native
  RotaryFixedConnection and TrimParameter processors remain unpatched; an
  audit only checks their original instructions. User takeover settings remain.
- Record up to 1024 timestamped raw K events in RAM; Mod Off exports the report.
- Preserve the complete OptoCat DSP, animation and Type controls, permanent K
  display, pad colours, spectrum/native curves, full-project restore, exFAT
  and experimental FAT32.

ARM/QEMU and UBSan cover all 768 knob positions in original order/on the input
thread without servicing the UI timer; r26h reproduces the unwanted deferral.
Checks include queued/in-flight/naming boundaries, compressor/resampling routes,
native takeover history, rendering, unchanged native processor instructions,
compressor DSP/animation/native adapter and the full preload constructor.
The legacy r26h helper is tested historically but is not linked into r26i.

This is a device-test candidate, not a confirmed resolution. Retest Start/End,
other knobs, Shift functions and custom controls. If stalled, wait 15 seconds,
use Project → Mod Off, then preserve the card report. Firmware **1.3.0.12**;
replace the complete `autorun/` folder or use installer 1.0.2. No formatting
or moving samples is needed. Not published; issue #2 remains open.

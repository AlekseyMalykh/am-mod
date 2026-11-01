# r26i — native knob input and takeover

[Русский](RELEASE_NOTES-r26i.ru.md) · [Issue #2](https://github.com/AlekseyMalykh/am-mod/issues/2). The author confirmed the K parameter fix on a physical MPC Sample on **2 November 2026** after testing the r26i SD build.

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

Hardware confirmation applies to the reported K parameter stall; it is not a
claim that every Shift function and custom control combination was tested.
The approved library is unchanged from the SD trial (SHA-256:
`b7cb3cc8cf6cd779b5426a0680b8fc9ec693fb4425cf8190d686583f51d4b1fa`).

Firmware **1.3.0.12**; replace the complete `autorun/` folder from an r26i
package or use installer 1.0.2. No formatting or moving samples is needed.
The r26i download has not yet been published; the current public r26d archive
does not contain this fix or FAT32 support.

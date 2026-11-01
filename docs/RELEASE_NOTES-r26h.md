# r26h candidate — Scaled limits and OptoCat

[Русский](RELEASE_NOTES-r26h.ru.md) · [Issue #2](https://github.com/AlekseyMalykh/am-mod/issues/2) still needs device confirmation.

- Apply exact Scaled endpoints even on the first event after a history reset,
  or a repeated endpoint with a stale intermediate parameter. Only exact 0/127
  on active, enabled Scaled bindings are corrected. Native range/skew/quantisation
  and parameter writes remain in use. Interior motion, Pickup and Jump are
  unchanged; no endpoint dead zone is introduced.
- Preserve **OptoCat**, including DSP, animation and Shift+Pad 5 / B2 Type,
  from the other development branch (`81189e0`). Both builders include it
  alongside the Scaled correction.
- Investigate permanent K display: executing the production painter between
  native takeover movements and expiring the native display timer did not
  change the parameter, Scaled history or focus in isolated ARM tests. Full GUI
  workload, notification ordering and physical input remain outside that proof.
- Add bounded raw endpoint and Scaled-state diagnostics for a device retest.

ARM/QEMU and UBSan cover actual native Pickup/Scaled/Jump, both limits, reversals,
initial values, skewed/quantised ranges; production painter with isolated
Graphics/value getter/text-formatting receivers; queue/input routes including
OptoCat; compressor DSP, animation and native audio adapter. The full combined
preload constructor installs all 50 callbacks. Complete Warp UI and physical
potentiometers were not exercised by these checks.

Firmware **1.3.0.12**, exFAT and **experimental FAT32**. Replace the full `autorun/`
folder or use installer 1.0.2. No formatting or moving samples/projects is required.
Full-project restore, pad colours, spectrum and native filter/envelope curves are
retained. This candidate is not published.

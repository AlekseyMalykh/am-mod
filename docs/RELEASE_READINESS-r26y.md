# r26y readiness

Unpublished local test candidate; public ZIP remains r26s.

See [changes](RELEASE_NOTES-r26y.md) and [settings/tests](MOD_SETTINGS.md).
ARM/QEMU/UBSan cover tabs, scrolling/reset, palette permutation and persistence,
XPJ display/Opto snapshots, failed/legacy reads, asynchronous recovery origin,
first-Save routing, waveform-only tint, retained curves, K, Repeat, Piano/Scale,
EQ, workflow colours and exclusive WRITE/TOGGLE. Full native VM execution is
recorded separately; receivers and emulator tests are not physical MPC confirmation.

Device gates: distinct A/B projects with notes/samples/tempo/effects and display/
compressor settings; manual Save/Load and power cycle; recovered name/folder and
first-Save Review/confirmation; tint and native curves; palette swaps/reset and
cursor patterns; Steps Undo/Redo and transport; original L02/L03, L06, L11 and
native Copy Shift+Bank reports. Physical FAT32 is still pending.

The author reported a successful prior project pass apart from the technical
recovery-copy name. That does not close every Leusha scenario or confirm r26y.

Full native VM confirms circular tabs, scrolling, reset confirmation/cancel,
no encoder double-click reset, and waveform tint. Recovery origin is captured
before the native Temp-Project handoff. After loading Mod-Exit-Last, native Save
shows Lifecycle-B / External, requires a separate confirmation and supports
Review. After cancelling Save and confirming normal power-off, the new copy
retains the original name/folder, notes [0, 960, 1920], tempo 137, volume 0.25 and
exact display/OptoCat/Warm snapshots. Original A/B XPJs remain byte-identical;
the native autosave destination stays Temp-Project. Verified library:
`5e0b5947134dce81cc6a8d5a90a166d55b51e570f3fd7da07cda76e0339a6d5b`.
These are VM results; r26y still needs physical device testing.

The early handoff is now implemented: splash before application preparation,
SD selection in the first native Load, and no late autoload. A cold native VM
boot confirms one SINGLE_LOAD, one PROJECT_READ_COMMITTED, no late AUTOLOAD,
stock stopped, retired early guard and unchanged A/B originals. The cold-loader
run used library `64f76a67e2a24a3dfdcfc7750a698cbc53b99b43b61a746ae75f0ae749dd0a0e`;
the subsequent build additionally settles a native empty New Project startup.
Only software-CPU deadlines were extended; these are not MPC timing measurements.
Final library `c72f838700b93668736078f9ea757e491e84bdbe67bf6fcc85f2d655e5210ca8`
also confirms a single Load, recovered Save name / External, confirmation cancel,
unchanged Temp-Project destination, exact display/OptoCat/Warm snapshots and
normal-power full recovery saving. Notes [0, 960, 1920], tempo 137, volume 0.25
and original identity survive; A/B source files remain unchanged. This candidate
has not been published as stable. [Boot flow/device checks (Russian)](BOOT_FLOW.ru.md).

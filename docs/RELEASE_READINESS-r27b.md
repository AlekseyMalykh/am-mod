# r27b readiness

Prepared locally; not published. Publish as a **prerelease** while the remaining
device scenarios are open. Public r26s is unchanged.

- Built library, preload constructors and relevant ARM/QEMU normal/UBSan checks pass.
- About is read-only, last of five pages; both directions wrap through it.
- Version/repository share one header across the helper, app splash and About.
- r27a visibility and the K fix are device-confirmed. r27b text needs a device check.
- Exact package/library/helper hashes and native VM evidence are recorded separately.
- L01–L04 are not all closed; keep L06, L11, native Copy reverse-bank and physical
  FAT32 checks open. Do not replace user reports with emulator-only closure.

Before stable: test r27b splash/About readability on MPC, A/B projects with distinct
notes/tempo/samples/effects/settings, normal shutdown/restoration, first recovered
Save/Cancel/Review, missing samples, empty card, and experimental FAT32.
Back up projects/samples. [Changes](RELEASE_NOTES-r27b.md).

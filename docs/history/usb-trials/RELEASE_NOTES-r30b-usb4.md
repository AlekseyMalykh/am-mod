# Am MOD r30b-usb4 — USB setup readiness and Screen Mirror

The usb3 hardware log confirms SD identification succeeded and the card reached gadget switching, then failed with **NO_MASS_STORAGE_LUN**. The stock switch command triggers setup asynchronously. usb4 explicitly waits for the `az0x-usb-gadget@ac50.service` setup job and bounded LUN/UDC readiness. Restore also waits for setup. Tests cover delayed creation and timeout rollback.

Screen Mirror now polls the active KMS framebuffer, brackets CPU reads with DMA-BUF synchronisation, and discards a snapshot if the active frame changes during capture. Quiet modal pages update without requiring another PageFlip. This addresses the reported new confirmation text over stale Project menu pixels in the mirror while the LCD is correct. A regression model exercises stale CPU cache, pending/presented frames, mid-copy changes and failures, with ASan/UBSan and AArch64 compilation.

Use Project → SD Card Access → Connect. Safely eject on the computer before unplugging USB; keep SD inserted. Audio/MIDI pause; the mod returns to Mod-Exit-Last. Browser transfers and Stereo / Master + A–H remain.

**A physical retry is still required.** The original failures are hardware-reported; successful USB disk transfer and mirror correction are not yet hardware-confirmed. Back up the card before testing.

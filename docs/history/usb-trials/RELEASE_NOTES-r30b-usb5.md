# Am MOD r30b-usb5 — leave SD Card Access on the MPC

Safely eject the disk on the computer, then press **B3 (Disconnect)** on the MPC and **B3 (Return to Am MOD)** again. B1 cancels confirmation. Leave the USB cable and SD card connected. This confirmation cannot flush the computer's file cache: finish copying and eject the disk first.

A RAM helper reads the internal raw MIDI surface while the main app is stopped. It requires fresh complete button clicks and ignores held Connect, repeated downs, unrelated messages and USB MIDI. The host closes USB ownership before mounting the card, then restores audio and the saved Mod-Exit-Last project. If the internal input cannot be identified, the screen retains unplug-to-return instructions.

The transfer page is painted before selecting scanout, replacing the boot-artwork-first path. Page changes issue a KMS commit rather than relying only on DIRTYFB.

Simulated ownership cycles, local exit with USB configured, input confirmation/cancel, held-button filtering, KMS commits, ASan/UBSan and AArch64 compilation are checked. **Physical button input and LCD refresh still need hardware validation.** usb4 hardware testing confirmed readable USB storage and project restoration after cable removal.

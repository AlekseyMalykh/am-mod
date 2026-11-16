# Am MOD r30b-usb3 — SD Card Access entry fix

Fixes the hardware-reported usb2 failure: Connect blanked the display, restarted the mod and showed USB transfer interrupted. All three logged attempts ended with **USB_DRIVE_ERROR NOT_REMOVABLE**, before the card was exported.

The SD controller no longer requires the block device's removable hint. It verifies MMC bus type **SD**, derives the partition from the actual mounted SD volume, and retains CID/device identity and exclusive-unmount checks. Internal eMMC (type MMC) is rejected even if removable=1; unknown card types are rejected too. Early rollback no longer tries to read an incomplete context. The error page now shows the failure code.

Use Project → SD Card Access → Connect; safely eject on the PC before unplugging. The mod restarts with Mod-Exit-Last. Audio/MIDI and the app pause while sharing. Existing browser transfer and Stereo / Master + A–H settings remain.

Regression coverage includes real-MPC-style SD with removable=0, internal eMMC rejection, missing type and rollback. The original failure is hardware-confirmed; **this correction still needs a physical retry**. Back up the card before testing.

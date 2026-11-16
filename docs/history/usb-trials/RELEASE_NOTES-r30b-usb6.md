# Am MOD r30b-usb6 — identify the SD Card Access button input

The usb5 device log contains two native raw MIDI inputs with the same `SerialMIDI` ID: card0 `Control Surface` and card1 `External MIDI`. The old filter accepted both and disabled local input as ambiguous, displaying unplug-to-return instructions.

usb6 requires the exact combination of ID `SerialMIDI` and name `Control Surface`. A regression fixture uses both physical device records. The RAM log also records up to 96 non-realtime input bytes to validate actual button messages.

Safely eject the disk on the PC, then press **B3 Disconnect → B3 Return to Am MOD** on the MPC. B1 cancels. Leave the SD card and cable connected. Restart the MPC after installation to load the updated RAM helpers.

Ownership tests, exact input selection, MIDI parsing, confirmation/cancel, ASan/UBSan and AArch64 compilation are checked. B3 still requires a physical retry after this fix. Cable-disconnect recovery is retained.

# Am MOD r30b-usb8 — actual B3 messages and Stock USB audio

usb7 logs confirmed the selected Control Surface input, timestamp-framed input inherited from the app, and B3 Note On/Off messages `90 0e 7f` / `80 0e 00` rather than CC.

The helper drops the old queue and explicitly sets RAWMIDI_PARAMS mode=0 before reading. The parser accepts channel-1 Note On/Off, including zero-velocity Note On releases. Regression checks replay real button payloads and cover confirmation/cancel and mode-setting failure. CC no longer triggers B3.

Safely eject the disk on the PC, then press **B3 Disconnect → B3 Return to Am MOD**. B1 cancels. Keep the cable and SD card connected. Restart the MPC after installation.

Stock USB audio displays locked Stereo, using the stock master output for phones/PCs. Am MOD offers Stereo or Master + A–H, applied after restart. Multichannel mode is one device with 18 channels: Master L/R 1–2, A L/R 3–4, B 5–6, …, H 17–18. The Am MOD preference is preserved in Stock. SD Card Access pauses audio/MIDI.

Regression checks, ASan/UBSan and AArch64 compilation are completed. B3 exit after these fixes, phone master recording and multichannel DAW recording still require physical validation.

# Am MOD r30b-usb10 — red important hints

Restart instructions for USB audio and FX mode, playback/USB-audio interruption, safe PC eject, and keeping the SD card inserted are highlighted in red. Descriptions retain grey/white and action labels retain amber. The SD Card Access confirmation now says to eject on the PC and use B3, replacing the outdated mandatory cable-unplug instruction.

Preserves usb9 input handling. The user physically confirmed project restoration via B3 → B3 without removing USB. A REAPER screenshot confirmed 18 enumerated channels; simultaneous recording of all stereo pairs remains unverified.

Individual USB channel names Master L/R and Track A–H L/R are not implemented. Firmware lists usb_f_uac2 as built into kernel 6.12.60-imb-2025-12-03-rt13. Its configuration exposes terminal/group names, not 18 individual channel strings. REAPER settings, Windows drivers and the MPC kernel are unchanged.

Restart the MPC after installation. Transfer: SD Card Access → Connect. Return: safely eject the disk on the PC → B3 → B3.

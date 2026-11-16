# USB in Am MOD r30b

[README](../README.md) · [User guide](USER_GUIDE.md) · [Русский](USB_GUIDE.ru.md)

## SD Card Access

1. Connect a USB data cable to the computer and leave the SD card inside the MPC.
2. **Project (Shift+Pad16) → SD Card Access → Connect**. Save the offered project copy if needed.
3. The app stops while the card is shared. Wait for its drive to appear. The MPC's **SD Card Access / Disconnect** screen is the transfer screen, not a stalled project.
4. Copy files; do not remove the physical card.
5. Close files and **safely eject the drive in the computer's OS**.
6. Press **B3 Disconnect → B3 Return** on the MPC, confirming that the computer has ejected the drive. The project reloads; the USB cable can stay connected.

B3 does not replace safe ejection on the computer. If a transfer is interrupted, let the app recover and check copied files. Keep separate backups of important projects.

## Updating through USB

Open SD Card Access and select the new ZIP and exported card drive in the installer. Wait for verification. Eject, return with B3 → B3, then **power the MPC off and on**. The old RAM instance keeps running until restart.

Only `autorun/` and version metadata are replaced; the previous `autorun/` is backed up on the computer. Projects and samples are left alone.

For a first installation on a clean card, use **stock** SD Card Access, install the complete `autorun/`, follow the stock USB disconnect prompt and restart. Updating an existing mod through USB has been tested on an MPC; the complete clean-card first-install scenario has not been separately confirmed on hardware. A card reader can also be used.

## Stereo Master for a phone/computer

Choose **Project → Mod Settings → Quick → USB audio → Stereo**, restart the MPC and connect it as a USB audio device. Stock always sends stereo Master, even if the card stores the multichannel preference. Am MOD's Stereo setting also sends Master L/R.

Select USB input in the phone's recording app if available. USB host support, power, adapters and app behaviour depend on the phone; universal compatibility is not claimed.

## Eight stereo tracks in a DAW

1. Set **FX mode → Am MOD**, **USB audio → Master + A–H**.
2. **Restart the MPC.** Changing the menu value alone does not change the USB channel count.
3. Reopen the DAW audio device; reconnect USB if needed.
4. In Windows REAPER with Akai Pro MPC Sample ASIO, enable the entire input range through **18**, then select:

| DAW inputs | MPC signal |
|---|---|
| 1/2 | Master L/R |
| 3/4 | Track A L/R |
| 5/6 | Track B L/R |
| 7/8 | Track C L/R |
| 9/10 | Track D L/R |
| 11/12 | Track E L/R |
| 13/14 | Track F L/R |
| 15/16 | Track G L/R |
| 17/18 | Track H L/R |

Names are supplied by the MPC's USB descriptors: `Master 1`, `Master 2`, `Track A 3`, `Track A 4`, etc. A DAW combines two mono channels into a stereo pair. Overlapping pairs such as Master 2 / Track A 3 are adjacent mono inputs, not extra buses. In REAPER, hide them with **Show non-standard stereo channel pairs** under Audio → Channel naming/mapping.

Track taps are after track processing/level and before Master FX; Master is the final output. Direct pads appear in Master. A send enters its destination before that track's FX, so track recordings may overlap. Do not sum Master with the separate tracks as independent stems. Playback from computer to MPC remains a stereo USB Return.

## Avoid monitoring feedback

When DAW playback returns over USB and the MPC monitors USB, **disable input monitoring on the DAW's recording tracks**. Otherwise Master returns through USB Return into Master again.

In Am MOD, Input → ROUTE chooses Input / Track / Master and Record FX Wet / Dry. Monitoring follows that choice. Sample Record B1 indicates Off / Record / On; Off stops listening without disabling input recording. Rear Routing also has independent Mon L/R controls.

## Troubleshooting

| Symptom | Check |
|---|---|
| Only two DAW inputs | Am MOD, Master + A–H, MPC restart, full DAW input range |
| Old/unusual names | Reopen the device; names come from MPC, REAPER aliases are unnecessary |
| USB transfer interrupted | Data cable, card inserted, diagnostic journal; do not pull the card during transfer |
| Project does not return after eject | Press B3 Disconnect → B3 Return on MPC |
| Old version after update | Restart the MPC after copying |
| Growing feedback | Break the DAW input monitoring ↔ MPC USB monitoring loop |

SD Card Access and USB audio are separate modes: the application and audio stop during drive access. The experimental **SD files via USB** browser service is separate and off by default; use SD Card Access for the full card drive.

# Am MOD r30b-usb1 — USB trial

Based on r30a, MPC Sample firmware **1.3.0.12** only. **Not hardware tested.** Back up projects and samples before installation.

Project → Mod Settings → Quick:

- **SD files via USB: On** enables **http://192.168.155.1:8082** over the MPC USB network. Browse/download regular files in Projects and Samples; upload new WAVs to **Samples/User/USB Inbox** (up to 1 GiB each, no overwrite). Off after reboot. This is a browser file service, not a mounted USB disk. Phone USB networking support varies. Save and stop recording before downloading a project; downloads are not an atomic multi-file project snapshot.
- **USB audio: Stereo** is the default and retains stock stereo Master and stock USB input. Recommended for phone recording; the actual phone/app still needs testing.
- **USB audio: Master + A–H** applies after a power cycle, in Am MOD FX mode. DAW inputs: 1–2 Master; 3–4 A; 5–6 B; 7–8 C; 9–10 D; 11–12 E; 13–14 F; 15–16 G; 17–18 H, at 44.1 kHz. Tracks are mirrored and remain audible locally. Track taps are after track processing, before Master FX; sends are part of receiving tracks, so stems can overlap. Master is tapped from the live mixer output. Its exact relation to stock stereo, compressor and Main Volume needs hardware verification. Missing mixer frames fall back to stock stereo Master with silent additional outputs. Queue latency, overflow and reconnect behaviour need real recording tests.

Use Stereo and restart before connecting a phone that cannot record multichannel audio. USB reconnects interrupt audio, MIDI, Screen Mirror and transfers. Enabling multichannel briefly reconnects USB at startup. Stock recovery restores the stereo gadget scheme. Firmware files are not rewritten: the modified scheme is overlaid from RAM only.

`verify_usb.py` exercises real HTTP requests and files, traversal/symlink rejection, incomplete upload cleanup, no overwrite, opt-in, concurrent audio queue with ASan/UBSan, the actual ALSA interposer against a separate fake library, partial writes and errors, simulated USB configuration/rollback, and AArch64 compilation. Full-library preload and existing regression checks are recorded separately. These are not hardware USB enumeration, audio synchronisation, DAW, phone or disconnect tests. Use a spare backed-up card for the first trial. An abrupt power loss may leave a hidden incomplete upload file.

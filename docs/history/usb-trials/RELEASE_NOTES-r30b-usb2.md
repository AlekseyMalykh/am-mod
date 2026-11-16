# Am MOD r30b-usb2 — SD Card Access trial

Based on r30a, retaining usb1 stereo/18-channel audio and browser transfers. Firmware **1.3.0.12** only. **Not hardware tested.** Back up the full card and use a spare test card.

**Project → SD Card Access → B3 Connect** stops playback, saves a separate project/sample copy as **Projects/Mod-Exit-Last.xpj**, stops the mod and its SD services, and exposes the unmounted SD volume through the stock USB Mass Storage gadget. The MPC shows a dedicated RAM-resident transfer screen. Use Explorer/Finder normally.

**Safely eject the disk on the computer.** A host Eject returns to the mod; if the OS does not send an eject command, unplug USB after safe removal. The disk is detached before MPC remounts it. The app restarts and loads the exact saved Mod-Exit-Last copy, regardless of newer XPJs uploaded by the computer. If no computer connects, the mode returns after two minutes. Do not remove the physical SD or delete/rename the recovery XPJ, its data folder, Samples, workspace or autorun during this session. Unplugging without safe removal cannot recover host-cached writes.

Playback, USB audio/MIDI, Screen Mirror and browser transfers are unavailable in disk mode. App restart replaces transport continuity. Browser transfers and Screen Mirror return Off. If the card is busy or gadget switching fails, rollback attempts to resume the saved project and displays a notice in Project. Force/lazy unmount is never used. A removed/replaced card or failed remount can prevent automatic return; disconnect USB and restart with the original card. Autorun updates copied by the PC take effect on the next ordinary boot, not during this RAM session.

Quick settings retain **USB audio: Stereo** (default, recommended for phones) / **Master + A–H** (restart, Am MOD mode, 18 channels at 44.1 kHz: Master 1–2, tracks 3–18), and **SD files via USB** at **http://192.168.155.1:8082**. Phone/DAW and USB-network compatibility still need hardware tests.

Ownership tests simulate mount/systemd/gadget operations, including busy/private mounts, internal-storage rejection, host eject, disconnect, no-host timeout, failed switching, card replacement and interrupted-export recovery. ARM/QEMU exercises native Project dispatch, exact resume selection, the save-copy worker, full preload and USB taps. The transfer screen is rendered from production code. This does not establish real USB enumeration, physical filesystem safety or OS compatibility.

# AM Mod r26a — 1 November 2026

Changes since public r25x:
- Pad Colour in Sample/Events and ordinary Steps events; native FX Select colours retained.
- Steps: one cursor, consistent screen frame/numbering, encoder toggles the frame too.
- Empty steps off; other sounds very dim white, default On and switchable with Shift+K3.
- Events: red B3 Erase deletes one row; Erase+pad retains native confirmation for the whole sound in the sequence.

The white background can occupy every pad of a page when other sounds have events on every step. Own events keep Pad Colour; selection/playback/Mute/Erase have priority.

Firmware: MPC Sample **1.3.0.12 only**. Copy `autorun/` to the SD root or use the installer. Save work to External. Mod Off returns the stock application; wait for it before Eject.

Published at the author's request after device trials. ARM/QEMU normal/UBSan tests, real native LED conversion, both LED senders, production screen drawing, event controls and preload construction passed. Automated fixtures capture some native UI/audio receivers and do not establish a full hardware test matrix.

The public ZIP carries the exact same autorun payload as the r26a candidate installed on the author's card; only documentation is refreshed. No firmware, samples, projects or device logs are distributed. See [verification](RELEASE-VERIFICATION-r26a.json), [English guide](FEATURES.md) and [Russian guide](FEATURES.ru.md).

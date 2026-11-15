# r29zg

- Fix (Save Project / Save Sample): "Insert SD card to save" appeared at random even with the card inserted, in both Stock and Am MOD mode. The mod read the "External storage" flags of the Save screen at the wrong position (0x11a0/0x11b8 instead of 0x10a0/0x10b8), so it saw an arbitrary byte, and when that byte happened to be zero it drew the message and swallowed the Save button. The correct flags are now used. The `SAVE_SCREEN` log line also prints the old positions (`old_offsets=`) for comparison.
- Includes r29zf (resample diagnostics `RESAMPLE_MIX PROJECT` / `RESAMPLE_MIX BUSES`).

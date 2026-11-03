# r27d — sample browser cursor

Local test candidate, unpublished. The public release remains r26s.

Fixed white selection staying on the pre-browser pad while the actual sound
and load destination changed. The cursor now reads the native load-target
index, independent of the cached instrument and hidden Sample editor label.
Search initializes its destination from the same index.

Preserves all eight banks, empty/occupied pads, Steady/Slow/Normal/Fast,
assigned colours, OptoCat/Warm and the other r27c fixes.

ARM/UBSan tests execute the real native setter for all128 pads, stale editor
selection, invalid indexes and search destination inheritance. The full VM
confirmed A01→A02→A06→A16 updates the target index while the cached instrument
stays A01. Visual confirmation of the new build on MPC is pending; a full
VM boot of this exact package is not claimed.

Test pad changes and bank changes inside the browser, empty/occupied slots,
search Load to, and steady/blinking selection. The previous pad must return
to its ordinary colour. Standard keeps native lighting.

**Back up samples and projects before updating.** Replace the complete
autorun folder without formatting. exFAT supported, FAT32 experimental.

[r27c changes](RELEASE_NOTES-r27c.md) · [Settings](MOD_SETTINGS.md).

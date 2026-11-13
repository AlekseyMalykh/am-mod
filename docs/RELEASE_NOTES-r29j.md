# r29j (test build after r29i)

**Fix: FX Select now really affects only the selected pads in Am MOD.**

- r29i did not take effect on the device: the stock routes of selected and unselected pads both point to other buffers, so the check by "main" never matched. r29j tells them apart by route id (selected pads: effect bus, unselected: main bus).
- When some pads are selected for FX Select (in Stock, saved in the project), those pads keep the stock path (stock effect, then Master) and the others go through the Mixer Tracks. When all or none are selected, all pads go through the Mixer as before.
- Selected pads bypass the Tracks (meters, Track FX) while selected.

[Русский](RELEASE_NOTES-r29j.ru.md)

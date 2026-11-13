# r29i (test build after r29h)

**Knob FX / FX Select now only affects the selected pads in Am MOD mode.**

- When **some** pads are selected for FX Select, the selected pads keep the stock path (stock effect, then Master); the other pads go through the Mixer Tracks as usual. The effect no longer touches unselected pads.
- When **all** pads are selected (B2 All) or none, nothing changes: pads go through the Mixer and the effect (if any) acts on the Master mix like a stock insert.
- Selected pads bypass the Tracks (and their meters and Track FX) while selected.
- Known: the first time an unselected pad is played after a selected one was already sounding, a long note of the selected pad can change path once. Please report any click you hear.

Includes r29h (`ROUTE_PADS` shows route ids) and everything before.

[Русский](RELEASE_NOTES-r29i.ru.md)

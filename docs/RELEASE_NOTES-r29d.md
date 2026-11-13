# r29d (test build after r29c)

New setting **Mod Settings → Quick → FX mode** (Stock / Am MOD). Default **Am MOD**. A change applies after a restart.

- **Stock:** the Mixer is switched off completely (no mod routing of pads). Knob FX, Shift+Knob FX, FX Select, Pad FX and Flex Beat are the stock ones.
- **Am MOD:** **Knob FX opens the Mixer** (press alone; the stock Knob FX curtain does not appear). The old Pad FX + Knob FX entry is gone. **Shift+Knob FX** opens the Mixer on the **Track FX page of the last pad you hit** (Master if it has no Track). Pad FX and Shift+Pad FX stay stock and process the whole Master. Inside the Mixer, Pad FX still assigns pads to Tracks.
- Why: stock FX Select merges the selected pads into one stream, which cannot work with per-pad Mixer routing (any selected pad made the effect hit every pad).
- Boot screen shows the current version (r29c fix). Includes r29b and r29c diagnostics (`ROUTE_PADS`).

Known limits: stock FX Select assignments already stored in a project are not removed in Am MOD (they are not shown and are not editable). Shift+Knob FX in Seq behaves like in Sample for now.

[Русский](RELEASE_NOTES-r29d.ru.md)

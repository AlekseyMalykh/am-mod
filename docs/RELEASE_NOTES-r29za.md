# r29za

- Project menu (Shift+Pad16): the native "SD Card Access" row is removed in both modes (USB card access cannot work inside the mod's isolated application copy; see README). Rows: Load, Save, New, Key / Scale / Tempo, Mod Settings, Screen Mirror.
- Mod Settings: the "Record via / Record track / Record FX" rows are not clickable in Stock mode (they only work in Am MOD); the hint says so.
- Diagnostics: `RESAMPLE_MIX HEAD` lines in the console log show the first blocks of the Am MOD sample-resample capture (peak per block), to find why the start of a pad sound can be lost.

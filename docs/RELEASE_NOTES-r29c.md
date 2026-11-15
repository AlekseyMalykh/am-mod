# r29c (test build after r29b)

- **Loading screen version:** the boot splash was taken unchanged from an old base build and showed an older version. It is now rebuilt with every package and shows the current version.
- **Diagnostics for Knob FX scope and pad mute:** the log gets `ROUTE_PADS` lines (which pads the stock firmware sends where). This is for finding why Knob FX can hit pads that are not selected; no audio behaviour changed yet.
- Includes r29b (Save External enforcement) and everything from [r29a](RELEASE_NOTES-r29a.md).

[Русский](RELEASE_NOTES-r29c.ru.md)

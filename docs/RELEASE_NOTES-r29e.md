# r29e (test build after r29d)

- **Shift+Knob FX outside Sample is now unavailable** (in Am MOD mode it no longer falls through to the stock action, e.g. Input Config in Seq). In Sample it still opens the Mixer on the Track FX of the last pad you hit.
- Includes r29d (FX mode Stock / Am MOD, default Am MOD) and the `ROUTE_PADS` diagnostics.

Known issue: a stock FX Select effect saved in a project (for example Vinyl Emulator) can still be heard in Am MOD mode, although the Mixer shows nothing. To remove it, switch FX mode to Stock, clear the effect, then go back. Being investigated.

[Русский](RELEASE_NOTES-r29e.ru.md)

# r29k (test build after r29j)

**Am MOD now ignores the stock FX Select effect saved in the project.**

- Per the sound-path scheme, in Am MOD the stock FX Select (and the Knob FX curtain) are not used. A project saved in Stock with an FX Select effect and selected pads no longer sounds with that effect in Am MOD: pads go to a Track or Direct, then Master.
- Technically: a pad routed to the FX Select bus is treated as routed to the Program main bus, and the Mixer mix always goes there.
- The selection stays saved in the project; Stock mode still applies it.
- r29i/r29j (pad-by-pad FX Select split) are dropped.

[Русский](RELEASE_NOTES-r29k.ru.md)

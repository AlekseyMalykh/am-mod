# Am MOD r30a

The first release with the mixer, the input channel, resampling through the mixer and the tuner. Firmware **1.3.0.12** only.

## Read this first

- **The "SD Card Access" item is hidden** in the Project menu (Shift+Pad 16) in both modes. USB transfer mode ("the card as a drive on your computer") cannot work while the mod runs: the app runs in an isolated copy without access to raw storage devices, and the card is in use by the mod. The item was removed for safety. Use a card reader; to get USB mode back, remove `autorun/` from the card and start the MPC.
- **Two modes: Stock and Am MOD.** Project → Mod Settings → Quick → FX mode, applied after a restart. *Stock*: the stock FX Select (one effect), no mixer. *Am MOD*: the mixer replaces FX Select; Knob FX opens the mixer; a stock FX Select effect saved in a project is ignored. Signal-path diagrams for both modes (EN/RU) are in the README.
- Back up projects and samples before updating. Save to **External**.

## What is new

- **Mixer**: tracks A–H and Master, four insert effects per track, sends, sidechain, Song stems to the SD card.
- **Input channel** (mixer pad 15): Level, Pan, Mute, Input FX; recording point **Input / Track A–H / Master**, **Wet / Dry**; monitoring follows the record point; Rear modes.
- **Resample** a pad (Shift+Pad 11 in Sample) or a sequence (Shift+Pad 11 in Seq) through the mixer, with a destination dialog, Replace confirmation, stop confirmation and independent bank handling.
- **Tuner**: input (mixer Input ROUTE, Sample Record B3) and the sample of any pad (Stop+Chop in Sample); B3 "Orig note" stores the original note.
- **Pad colour from Track** on every screen with pad sounds.
- New illustrated effect screens, typing names with the pads, Master compressor OptoCat/Duck.

## Bugs fixed

- Save Project / Save Sample: false "Insert SD card" and saving to Internal.
- Stems export with an External Export window; the Stems name window now accepts pads at once.
- Resampled pad lost its attack; sequence resample skipped the mixer and could be silent at first start.
- Recording through Track or Master stayed dry.
- A stock FX Select effect sounded in Am MOD; one selected pad made Knob FX hit every pad.
- Pad colours turned stock under native dialogs.
- Resample dialog: Pad Bank stepping, bank letters, stop confirmation, Replace prompt with red target and hint (sequences), destination bank independent of the sequence bank.
- Compressor pad pick showed free-green / occupied-red like a destination: now a normal blinking selection.
- Seq+Sample window lit pads differently from Sample: now the same rules.
- Shift+Erase in the mixer started the stock copy gesture; K3 Volume in the sample library blinked; Master FX pinned Sample Comp row.

## Known

- Rare short crackle in the tuner window on very low notes.
- A missing sequence cursor in Song was seen once and could not be reproduced.

## Thanks

Huge thanks to **Leusha** for careful, detailed testing and for finding bugs: most of the fixes above started from those reports. Thanks also to mxt1iga and everyone who tested builds.

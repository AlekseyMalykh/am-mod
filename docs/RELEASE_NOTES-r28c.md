# Am MOD r28c · experimental release

For Akai MPC Sample firmware **1.3.0.12**. Back up projects and samples, then select `am-mod-r28c.zip` manually in the installer. For manual installation, copy the complete `autorun/` directory to the root of the SD card.

## New features

- **Screen Mirror.** Open **Project → Mod Settings → Pads → Screen Mirror** and press DATA or turn K1 to On. Connect the MPC to a computer or tablet with a USB-C data cable and open `http://192.168.155.1:8080/`. The browser shows the MPC screen. Screen Mirror starts in Off after each restart.
- **RGB palette.** In **Project → Mod Settings → Colours**, open **RGB Palette Editor**. Choose a cell with DATA or the corresponding pad in the 4 × 4 grid. Adjust RGB with K1–K3 and preview the colour immediately on its pad. Press **B2 Name**, choose Encoder or Pads with K1, then press **B3 Done**. Switch cells freely to edit several colours. **B3 Save** applies all changes and stores them on the SD card. **B1 Cancel** discards the drafts. The grid starts with 14 factory colours and two empty cells. Empty cells leave their pads dark. The extra colours become available for Sample pads and sequences after saving.
- **Pad Lock.** In the Mixer, choose a track and press **Pad FX**. With Pad Lock on, you can assign a pad that feeds Master directly. Press **B2** to unlock before removing or changing an existing A–H assignment. Pad Lock turns back on each time you enter Pad FX.
- **Mixer shortcuts.** In the overview, **Shift + B3 Route** opens the routing view. **Shift + Pad FX** opens it too. In **Knob FX → FX Chain**, **B2** copies an effect, **Shift + B2** pastes, **B3** toggles Solo and **Shift + B3** toggles Chain Bypass. In the Graphic EQ editor, **Shift + B3** adds a point.

## Fixes

- Solo keeps sends feeding the receiving track. A source outside Solo can still feed a soloed track while its direct output to Master is gated. Explicit Mute on the source also cuts its send.
- **Save Project** and **Save Sample** use the SD card. K3 no longer switches storage there or in **Song Export**. **Mixdown** and **Stems** write to `MPC-Sample/Samples/Export` on the card. Use **Save Project** to retain recordings, resampled pads and sequences created via **Song → Sequence**. Internal projects and samples can still be opened.
- The long-name field scrolls around the insertion cursor during pad typing.
- Press **Sample** to return from **16 Levels** or **Chop** to Sample.

Screen Mirror was confirmed on a physical MPC Sample with the final preview. The mixer, save dialogs, palette and button handling passed focused ARM checks. The full save-and-restart workflow and Sample exit from 16 Levels and Chop still need device testing. The interface is still being refined.

[Previous release notes](RELEASE_NOTES-r28b.md) · [Русский](RELEASE_NOTES-r28c.ru.md)

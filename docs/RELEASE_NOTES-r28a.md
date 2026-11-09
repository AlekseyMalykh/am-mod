# Am MOD r28a · experimental release

For Akai MPC Sample firmware **1.3.0.12**. The mixer, effect editors and export screens are still being refined. Back up projects and samples before updating. Select `am-mod-r28a.zip` manually in the installer, or copy the complete `autorun/` directory to the root of the SD card.

## What's new

- Mixer with eight tracks A–H and Master, grouped fader moves, pan, mute, solo, sends, track names and undo/redo.
- Up to four inserts per track or Master, including built-in effects, a three-point Graphic EQ and a Sidechain Compressor with a selectable key track.
- Song stem export with or without mixer effects. Silent buses are skipped. WAVs share a common start.
- In Project → Key / Scale / Tempo, K3 sets whole BPM and Shift + K3 adjusts hundredths of a BPM.
- An up arrow beside a knob label marks an alternate Shift control. Parameter automation recording is planned.

## Mixer and effects

1. Press **Pad FX + Knob FX** together. Pads **1–8** select tracks A–H, and pad **16** selects Master. Hold several track pads together or use Shift + pad to edit a group.
2. Select a track and press **Pad FX** to assign loaded sounds. Press a sound pad again to route it directly to Master. Track outputs also feed Master.
3. **K1** sets pan, **K2** sets send level, **Shift + K2** selects the destination, and **K3** sets track colour. The letter under FX identifies the destination. The red meter shows the incoming send on the receiving track.
4. The group fader picks up the current levels and preserves the offsets in dB. Hold **Shift** while moving it to align the levels. **B2** mutes, **B3** solos, **Shift + B2** names a single selected track, and **Shift + − / +** undoes or redoes a change.
5. Select a single track or Master and press **Knob FX**. Turn DATA to choose a slot, then press it to open the insert or the catalog at the first empty slot. Pads **9–12** open those slots directly. In the list, **B2/B3** copy/paste, **Erase → B3** deletes, **Shift + turn DATA** swaps adjacent inserts, and **Shift + press DATA** toggles bypass.
6. In the editor, **B1** returns to the list, **B2** toggles bypass, and **Shift + K1–K3** accesses the alternate parameters. Press DATA to cycle pages when available. In Graphic EQ, **Shift + B3** adds a point and turning DATA selects it. In the Sidechain Compressor, choose the trigger with **Key Track**. The white marker on its Key meter shows the threshold in dBFS.

## Export stems

1. In **Song**, press **B1 Export**. Turn DATA to choose **Stems with FX** or **Stems without FX**, then confirm with **B3 Do It**.
2. Name the export. **K1** switches between encoder and pad entry. Confirm **Do It** and wait for rendering to finish.
3. Look beside the full mixdown WAV for a folder with the same name. It contains 24-bit stereo files named `Name - track A.wav` through `Name - track H.wav` for audible tracks and `Name - Master.wav` for audible signals routed directly to Master. Use a new folder name for another export.

**Stems without FX** bypasses mixer insert chains and keeps pad processing. Import the track files and the direct Master file at the same position in your DAW. Keep the full mixdown WAV as a reference.

## Fixes

- Holding **Tap Tempo** in Sample or Seq preserves the Am MOD pad colours.
- Sample pads and waveform indicate track mute and solo. Muted pads alternate with red, soloed pads with green, and pads excluded by another track's solo are dimmed.
- **Sample → B1 Mix** shows **Pad Colour** and its swatch again. Turn **K2 without Shift** with Am MOD lighting enabled.
- Mixer pads **9–12** light up for occupied FX slots. Empty slots and all four pads in a group selection are grey.
- OptoCat knobs follow the selected **Parameter Takeover** mode on both normal and Shift layers.
- Fixed the **Song** heading, mixer button labels and the overlapping DATA hint in the effect catalog.
- Addressed the first **B3** confirmation after **Erase + pad** in Seq. In Steps, page, step position, play position and Follow/Hold stay visible together, including while a message is shown.

Focused mixer, effect, routing and Steps display checks passed in ARM emulation. The broader Steps test stopped at a Mute pad-colour assertion. The final r28a package has yet to be tested on hardware.

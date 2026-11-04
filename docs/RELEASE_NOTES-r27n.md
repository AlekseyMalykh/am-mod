# AM Mod r27n

Public prerelease for MPC Sample firmware **1.3.0.12**. Stable remains **r26s**.

- **Preview from end.** Hold **STOP** and press a pad in Sample or Chop to hear the end of the trimmed sample or slice. In **Project → Mod Settings → B3 → Sample → Preview from end**, choose 100–4000 ms. The default is 1000 ms. The setting is saved with the project and in card defaults.
- **Steps.** K1 sets the sequence length in bars, K2 sets the grid, and Shift+K1 is free. Use **SOUND + pad** to choose the sound and **PAD BANK** to switch banks. Available empty steps are dim, steps with events are brighter, and pads beyond the end stay dark. Mute shows audible steps in green, muted steps in red and mixed steps alternating.
- **Sample lighting.** A playing loop flashes in its assigned colour after you select another pad. Pressing SHIFT alone keeps the pad brightness. While using Mute, audible and muted pads are green and red. Muted pads have a grey waveform and spectrum, with filter/EQ curves and envelopes still visible.
- **Palette.** Eight assignable colours plus Default for samples and sequences. Six separate system colours remain swappable in Mod Settings.
- **Copy Pad and recovery.** Temporary audio uses a dedicated SD workspace to avoid false “Insufficient space on drive” errors with large samples. A successful named Save is recorded for project recovery.

**Testing.** The author confirmed STOP + pad and the Steps changes on MPC. Sample/Chop tail playback and offset persistence, loop and mute lighting, and large-sample copying were checked on the emulator. The original long-copy and named-save recovery scenarios still need targeted checks on MPC.

**Install.** Back up your samples and projects. Copy the complete `autorun/` folder to the card root or select the downloaded r27n ZIP in Windows Installer 1.0.3. exFAT is supported and FAT32 remains experimental. See the [installation guide](../README.md#install) and [testing status](KNOWN_ISSUES.md).

Thanks to Leusha and everyone who reported issues and tested the builds.

# Am MOD r27r — lighting, Recall and search fixes

MPC Sample **1.3.0.12**. Changes since r27q; includes all its features and fixes. Almost every fix here started from Leusha's careful tests — thank you!

## Fixes

- **Recall (Shift + Sample Record) no longer freezes the MPC.** Only Stop now waits for the audio render; Recall and other buttons reach the stock app directly. Checked on the MPC with Resample and Rear sources.
- **Chop lighting.** Pads stay lit when you enter Chop, after Extract and after leaving Chop, in the colour of the source pad. Inside Chop the mod no longer asks the stock app to repaint the pads (it answered with dark pads).
- **Seq Colour (Shift+K1)** changes the sequence colour again, also after Knob FX.
- **Scaled Knob Takeover** no longer jumps when you return from EQ to Filter.
- **Mix encoder cycle** goes Volume → Pad Colour → Pan. With Shift, Volume and Pan stay on screen and only K2 becomes Kit Volume, so Shift + encoder steps back through all three.
- **Screen after autoload** is complete without touching a pad; the “Restored” note is drawn cleanly and disappears on time.
- **Search on the SD card** finds all files (up to 4096 folders and 200 000 entries) and scans faster.

## Pad lighting: one set of rules

Colours are set in **Project → Mod Settings → Colours** (Cursor / selection, Free target, Occupied target, Muted, Play position); each pad's own colour is Sample → Mix → K2.

- **Cursor / selected pad** blinks white (Mod Settings → Cursor; Steady keeps it white). In Seq the selected sequence blinks white too.
- **Copy** (Shift+Erase) in Sample and **Copy Sequence** in Seq: the source blinks white and its own colour; chosen targets are steady green when free and red when occupied; other pads are dim in their own colour. After Copy Sequence the target gets the source's colour.
- **Sample:** loaded pads are dimmer at rest and full brightness while pressed or playing, several at once.
- **Mute:** pads keep their colours and get brighter, muted pads are red — also in the browser. Outside Mute, muted pads are grey.
- **Holding Erase** keeps your colours.
- **Pad Bank:** a short press changes the bank without the stock green flash; the bank picker appears after a 0.3 s hold.
- **Resample → B3 Sample:** the source pad lights immediately.
- **Startup:** while a project loads, one pad runs across the pads (13→16, 9→12, 5→8, 1→4) in the Play position colour.

## Windows and controls

- The **Lights** tab is now called **Scales** (Sample → Tune → Play → Scales).
- **Search:** K2 Search in: Folder / Library / **Internal** / SD Card / **All** (Internal + SD card). B2 does nothing while typing (it duplicated Find). K3 Volume no longer flickers while scrolling. The Internal / External tab uses the stock font and position dots. Knob FX (also Shift+Pad16) does not open over the browser.
- **Sample and Seq** leave Mod Settings, Key / Scale, the Mod Off question, Search and Stereo > Mono and keep their stock action; Batch closes on the first press.
- **Pad Bank + K** also resets the browser preview volume and Search in, and Rear Routing pan / level.
- **Filter / Filt Env spectrum** uses the same colour as EQ.
- **Preview from end** (hold STOP + pad) silences the sounding pad before its end plays — like a double Stop, all sounding pads stop — and can be turned **Off** in Mod Settings → Sample. Off replaces the former 4000 ms choice.

## Known limits

- Sample and Seq do not leave 16 Levels or Chop yet (stock behaviour). This release logs button codes so it can be added next.
- A muted pad shows no spectrum: it does not play.
- Pad Bank + K does not reset Seq, FX, Input Config or Knob FX parameters.

## Tested

ARM builds pass the full preload check over all 51 native hooks, the routing / lighting / knob host tests and the DRM splash checks. On the MPC the author confirmed Recall, Chop / Extract / Save, Copy in Sample, Mute, the selected sequence, Pad Bank, resets in the browser and Rear Routing, search, Mix with Shift and the startup animation. Copy Sequence colour transfer and leaving windows with Sample / Seq need wider testing.

## Install

Back up projects and samples. In Installer 1.0.3 select `am-mod-r27r.zip` or *Latest release*, or copy the complete `autorun/` folder from the ZIP to the card root.

Thanks to **Leusha** for careful, patient testing and precise reports, and to everyone who tests the mod.

[Known issues](KNOWN_ISSUES.md) · [r27q changes](RELEASE_NOTES-r27q.md)

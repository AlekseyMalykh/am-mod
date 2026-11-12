# Am MOD r28d

Release for Akai MPC Sample firmware **1.3.0.12**, 12 November 2026.

r28d includes r28a–r28c features and the fixes from the interface audit. Mixer and effect window design is still being refined.

## Installation

Back up projects and samples. Select `am-mod-r28d.zip` in MPC Sample Mod Installer, or copy its complete `autorun/` folder to the SD card root, replacing the previous mod files. Restart with the card inserted and check r28d in About.

Projects, samples, Mixdown and stems save to SD. Save Project retains recordings and resampled pads. Existing Internal projects and samples can be opened.

## Included r28 features

- Eight tracks A–H and Master, groups, naming, pan, level, sends, mute/solo and undo/redo.
- Four insert slots per track, native effects, effect copy/paste, reordering and bypass.
- Graphic EQ with up to four points and a Sidechain Compressor whose Key drives the detector.
- Song Mixdown and aligned stems with or without mixer FX, skipping silent outputs.
- USB Screen Mirror, a named 4 × 4 RGB palette, global project tempo and OptoCat takeover.

## Changes in r28d

### Song

The list reserves room for the K footer, with Repeats on K1 and a persistent step number. Numbered rows and explicit repeat counts clarify repeated sequences. DATA and +/− select steps and stop at both ends.

Shift + B2 opens Clear All, with B3 confirmation and B1 cancellation. Project sequences remain available. Shift + B3 duplicates the current step, and Shift + +/− moves it. Song Export has updated controls. Exporting an entirely silent song reports an empty result instead of success.

Sends are part of the receiving track audio and can occur in several stems, allowing a separate reverb return. Master contains the final mix, including direct pads and track outputs. Account for this duplication when summing exported files.

### Mixer and effects

- Group level movement continues when an individual track reaches a limit. Off tracks can rise again. Shift + fader aligns group levels.
- DATA level edits preserve group level differences. Shift aligns the levels.
- Clicking DATA cycles Level → Pan → Send → Colour. Master offers Level and Pan. The active fader is amber and K controls have scales.
- A horizontal Master fader appears below its meter. The physical fader also works in Knob FX.
- Overview controls are B2 Route, Shift + B2 Rename, B3 Solo, Shift + B3 Clear Solo. Mute toggles the selection, Shift + Mute clears all track mutes.
- FX Chain uses B2 Copy, Shift + B2 Paste, B3 Solo and Shift + B3 Chain Bypass. Press DATA to open an effect, B1 to return to the list.
- Graphic EQ supports four points. DATA selects a point and Shift + B3 adds one. The add caption dims at the limit. Erase asks to delete the selected point, B3 confirms and B1 cancels.
- Erase in the FX list asks to remove the effect. The deletion dialog no longer covers the B header with a black rectangle.
- Pad Lock blocks all pad assignment changes, including pads feeding Master directly. B2 unlocks until Pad FX closes. Other tracks' pads blink red.
- Bank A–H LEDs are off in Mixer overview and Knob FX. Pad FX and Route retain the current pad bank, and leaving Mixer restores native lights.
- Conflicting Shift + +/− Route actions were removed. Group selections display unavailable actions and FX slots appropriately.

### Other fixes

- Project, Mod Settings, MIDI Config and Input Config retain Am MOD/Off lighting rules.
- RGB names save correctly. Unavailable SD storage leaves edits pending with an error. RGB preview is live, selecting another pad retains its draft, and B3 Save applies the whole palette.
- Overlapping or very dark system colours fall back to factory service colours, with an explanation in Mod Settings.
- Pad Bank + parameter fully updates the value and its display, including Trim End.
- Opening OptoCat preserves Bypass. Knob focus and Pickup guidance were added.
- Rear Routing labels distinguish dry recording from monitor Pan/Level. Editing parameters no longer enables monitoring. Queued controls retain their Shift state.
- Resampling fixes first-pad encoder selection, repeated bank selection and the stop confirmation. B3 Stop & Render proceeds, B1 cancels.
- Batch Copy/Extract fixes encoder direction and source/target navigation. Event-copy confirmation explains that all sequences are affected.
- 16 Levels clarifies Piano colour roles and Transpose16 units. Global tempo reaches its upper limit.
- B/K panels, captions, focus and hints follow shared conventions. Up arrows show Shift controls, active Solo is green and Bypass is red.

## Screen Mirror and RGB

Screen Mirror is at Project → Mod Settings → Pads → Screen Mirror. Enable it, connect a USB-C data cable and open `http://192.168.155.1:8080/`. It returns to Off after restarting.

Open Project → Mod Settings → Colours → RGB Palette Editor. Select a cell by pad or DATA, edit RGB with K1–K3, and use B2 Name for encoder or pad text entry. B3 Done finishes naming, B3 Save stores all edited cells, and B1 Cancel discards drafts. Empty cells stay dark.

## Verification and limits

The ARM build, pinned full preload constructor under QEMU, and focused Song, mixer, EQ, lighting, palette, reset, compressor and resampling checks passed. Package checksums and installer parsing were verified separately.

Screen Mirror was confirmed by the user on r28c hardware. Final r28d still needs physical MPC testing, including save/restart and audio scenarios with multiple sends. Mixer parameter automation recording remains planned.

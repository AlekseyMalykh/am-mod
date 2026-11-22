# Am MOD r30d user guide

[README](../README.md) · [USB](USB_GUIDE.md) · [Русский](USER_GUIDE.ru.md)

## Mixer

In Am MOD press **Knob FX**. On Sample, **Shift+Knob FX** opens FX for the last played pad's track, or Master for Direct pads. Pads 1–8 select A–H, 16 Master, 15 Input. Hold one track pad and tap others, or use Shift, for a group. B1 returns to Sample. Mixer entry from the compressor is disabled; open it through Sample.

Fader changes level; grouped tracks retain relative levels, Shift+fader aligns them. K1 is Pan, K2 Send (Shift: Send To), K3 Colour. DATA click selects Level / Pan / Send / Colour; turning edits it. Mute / Shift+Mute mutes / clears all mutes; overview B3 / Shift+B3 solos / clears solos. Shift+B2 renames. ± moves through A–H and Master.

**Pad FX → Assign Pads:** press a pad to assign it to the selected track; press again for Direct. B2 Pad Lock prevents edits. Assignment banks are independent of sequence banks.

**B2 Route:** Pads / Sends / Sidechain tabs. Sends provides From / To / Send. Encoder/knob choices omit the source and every destination that would form a feedback cycle, also for groups. Sidechain Key is a detector link, not an audio send. Sends enter destination tracks before their FX.

## FX Chain

Track, Master and Input each have four insert slots; pads 9–12 open them. DATA selects a slot; click opens its editor or the Empty slot's catalogue. K3 filters catalogue categories. Shift+DATA rotation in the list reorders effects.

Processing runs **serially from top to bottom**. An edit in the first effect reaches the second effect's input. EQ's live spectrum shows the selected slot's processed signal; the yellow curve shows only that EQ's own settings. Bypass stops effect animations/live spectrum while the track meter keeps showing passing audio.

| Control | Action |
|---|---|
| K1–K3 / Shift+K | Current page / alternate layer |
| DATA in editor | Named pages; EQ points |
| B1 FX List | Return to chain |
| B2 Bypass | Selected effect bypass |
| Shift+B3 | Whole chain bypass |
| Shift+Erase / Shift+B2 | Copy / Paste effect |
| Erase | Delete with confirmation |
| Shift+DATA in EQ | Add a point, up to four |
| Pads 1–8 / 15 / 16 | Selected Track / Input / Master FX list |

Main controls come first: Delay Time/Feedback/Mix, modulation Rate/Depth/Mix. [Complete map](FX_KNOB_LAYOUT.ru.md). Amp Model is on K1; [voice details](AMP_VOICES.ru.md). Separate Parameter / Knob FX / Fader takeover settings support Pickup, Scaled and Jump.

## Compressor

On Sample, **Shift+Pad5** opens Duck / OptoCat; B2 switches type. Both initially focus K1. DATA rotation edits the selected value, click advances K1 → K2 → K3 cyclically, and Shift+click reverses. Holding Shift changes the layer without latching a separate page.

Duck: Attack / Release / Amount, with Shift changing the third column to In Boost. OptoCat: Threshold / Ratio / Gain, Shift Attack / Release / Knee; B1 Warm, B3 Bypass. The pinned Master Sample Comp refers to the same stock compressor; it is not an additional serial instance.

## Input channel and recording

*Am MOD mode.* The **Input channel** is where Mic, USB and Rear enter the mixer. Open it with **pad 15** in the mixer.

| Control | Action |
|---|---|
| **Fader** | input level |
| **Mute** | mute the input |
| **B1 Back** | to the overview |
| **B2 FX** / **B3 ROUTE** | the Input FX chain (four slots, as on a track) / the routing list |
| **Shift+B2 Paste** / **Shift+B3 Chain Bypass** | paste an effect / bypass the whole chain |

**ROUTE rows.** Turn the encoder to choose a row, **click** to edit it, turn to change, click to accept.

| Row | Meaning |
|---|---|
| **Tuner >** | opens the [tuner](#tuner) |
| **Monitor** | Off / Auto / On: hear the input |
| **Record via** | **Input**, **Track** or **Master**: where the recording takes the signal |
| **Record track** | track A–H when *Record via* is Track |
| **Record FX** | **Dry**: no effects. **Wet**: the effects up to the chosen point (Track: Input FX, the track's effects, fader and pan; Master: and the Master effects) |
| **Rear mode** | only for the Rear source: Stereo, Rear L, Rear R, L+R Split, L+R Mix |

K2 is the input Pan (for Rear: K2/K3 are Pan L/R, with **Shift** Level L/R). Monitoring hears the signal from the same point that is recorded. Record on the stock **Sample Record** screen as usual: *Tap pad to record*. The settings are the same for Mic, USB and Rear.

## Resampling

Resampling renders what you hear into a new sample. In Am MOD it goes through the mixer (track fader, Mute/Solo, Track FX, sends, Master FX, the compressor); in Stock through the stock FX Select. The level of the result depends on the faders.

1. **A pad:** in the Sample screen select the pad with the sound, press **Shift+Pad 11** (the pad is the source, no question is asked). **A sequence:** in the Sequence screen press **Shift+Pad 11** (title *Resample Seq*).
2. The **destination page** opens. Pick the target pad:
   - a **free pad** renders at once;
   - a **recorded pad** shows *Replace?*: press the same pad again or **B3 Replace** to overwrite, **B1 Back** (or another pad) to change your mind. The target is shown red on the screen and on the pad.
3. **Pad Bank** steps the bank forward, **Shift+Pad Bank** back; **hold Pad Bank** and press pads 1–8 (red) to jump to a bank. The bank letters follow the dialog. The destination bank is independent of the sequence bank: after finishing or cancelling you return to your sequence bank.
4. If the transport is running, a window asks *Stop playback?*: **B3 Stop & Render** stops and starts, **B1 Cancel** keeps playing.
5. Progress is shown; cancel with **B1**. The result lands on the target pad.

## Tuner

One full-screen window, the same everywhere: note names, a straight ±50 cent scale with a moving needle (green when within ±5 cents), the frequency in Hz and the note with its octave.

- **Input signal:** mixer → Input → ROUTE → **Tuner** (DATA click), or **B3** on the stock *Sample Record* screen (the label *Tuner* is drawn next to the threshold icon). **B1**, **B2**, **B3** or DATA close it.
- **A pad's sample:** in the Sample screen hold **Stop** and press **Chop** (**Stop+Chop**). The window opens over the Sample screen: hit any pad and the window shows the pitch of that pad's sample (*Sample A05*). Percussive or noisy samples show *No pitch found*; notes below about 60 Hz are not detected. **B3 "Orig note"** stores the detected note as the sample's **Original note** (pitch class only; the pitch is not changed). **B1**, **B2** or **Chop** close it.


## Sample Record and Input Config

Sample Record B1 selects Off / Record / On; only Off has a crossed-out icon. Rear Routing has separate Mon L/R controls. In Am MOD use Input FX and Input ROUTE's Record FX Wet / Dry; the stock **Rec Input Effects** row is hidden. Stock retains that row. In Routing, Stop finishes/cancels local recording and reaches the transport; the second Stop kills voices and FX tails.

## r30c development: Piano Roll and sequence mutes

In Piano Roll, select a row and press **Mute** to mute or unmute that pad in the current sequence. A red **M** marks the row. In Tune and Chop layers, the mute belongs to the source pad and is shown beside all its rows. Events and other pads' or sequences' mute states are preserved.

A red **SM** means that the current sequence contains at least one muted pad. It appears in Piano Roll, the shared Sample/Sequence header, and ordinary Mixer/FX screens. Separate modal dialogs may use their own headers. Switching sequences shows the newly selected sequence's state.

While browsing Piano Roll, **K2** selects the grid and displays its current value directly above the knob. There are ten straight/triplet divisions and **Off / 1 tick**. Grid selection leaves zoom unchanged; Shift+DATA controls horizontal zoom. During event editing, K2 retains its Pad/Pitch function, or Velocity with Shift.

These changes are being prepared for r30c; source availability does not mean they are already installed on your MPC.

## Saving, export and help

The r30c candidate supports **Internal / External** through the native **K3** Storage control in Save Project, Save Sample and Song Export. Internal user projects and samples persist on flash rather than RAM. The writable user directories are `UserContent/Projects` and `UserContent/Samples/User`, including `Export`; other system data stays isolated.

Recovery retains the original volume, name and nested project folder. The first recovered Save requires a separate confirmation; a changed name or volume remains Save As. Missing folders or legacy metadata with an unknown origin produce a notice rather than selecting a same-named project on another volume. Recovery audio and native Temp autosave are not redirected into the original project.

Song → Export → Stems writes tracks and Master to the chosen volume's `<song>/` export folder; sends make stems overlap. MIDI files currently use a separate SD export path. See [USB](USB_GUIDE.md) for separate USB tracks.

**Shift+Sample → Input Config → B3 MIDI In** opens the shared MIDI input-channel settings. **B2 Map / Learn** opens mapping and learning. B1 returns from Map to MIDI Input, then to Input Config. These edit the same settings as MIDI Tools.

On MIDI Input, **B3 Mode** cycles **Drum map → Channels → Selected pad** without erasing assignments. Drum map keeps ordinary keyboard-to-pad mapping. Channels assigns a pad and Tune/Chop role to each incoming channel. Selected pad plays the pad currently selected on the MPC chromatically: K1 chooses the incoming channel (All or 1–16), K3 sets the root note. Select another MPC pad to change the sound; for channel-specific chops use Channels. **Shift+B3** clears input-mode settings and restores Drum map. Map/Learn assignments are separate.

[Settings](MOD_SETTINGS.md) · [Limitations](KNOWN_ISSUES.md) · [Report a bug](FEEDBACK.md).

## Synth, Input and Piano Roll (r30d)

Full details: [Synth guide](SYNTH_GUIDE.md). Mixer pad 13 opens the synth; Loop + pad (13–16, 9) or + / − picks a module, DATA flips pages, Shift+K edits the hidden parameter. K1 on Keys is Transpose16, Shift+K1 is Layout. Shift+Sample opens Input ROUTE from any screen. In Piano Roll hold DATA, turn the encoder and move the fader to select an area. **Mod Settings → About → Exit to bootloader** saves a project copy and returns to the D.S.B menu (or stock); see [Boot menus](BOOTLOADERS.md).

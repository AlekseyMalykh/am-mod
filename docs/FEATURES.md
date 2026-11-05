# Am MOD features

**English** · [Русский](FEATURES.ru.md)

Latest release **r27p**, 5 November 2026. MPC Sample firmware **1.3.0.12**.

**Statuses:**
- **Confirmed**: checked on a real MPC Sample.
- **Trial**: the specified automated checks passed; hardware coverage is pending as noted.

A confirmation covers that specific trial, not every combination of features in later builds. Build history: [CHANGELOG](../CHANGELOG.md).

Terms: **B1–B3** are the three buttons above the screen, **K1–K3** the three knobs below it, "encoder" is the large DATA wheel.

## r27f prerelease

The **r27f** prerelease combines the earlier changes and the device-confirmed
temporary-workspace fix for sample loading, Copy Pad and render assignment.
Remaining hardware scenarios are tracked separately.

- **About** is the fifth and last read-only settings tab. Splash/About show
  the same version and repository; B2/B3 wrap through all five pages.
- **Project → Mod Settings** replaces Pad Lighting: B2/B3 tabs, encoder
  row/value, B1 back. **Restore recommended** is last in each editable section.
  Project has six rows; **Mod Off is removed**.
- Standard / Am MOD; independent waveform/spectrum pad tint; service/user
  colour swaps retaining five user slots; Steady / Slow / Normal / Fast cursor.
- **XPJ stores display and compressor settings**, including the palette,
  tint, pulse, Type and OptoCat/Warm controls. Native Duck controls and Bypass
  remain owned by the native serializer.
- Normal power-off still saves a recovery copy. Restoration retains the
  original name/folder and requires a separate first-Save confirmation.
- Minimal **Am MOD** splash. Steps: SEQ RECORD keeps WRITE; SAMPLE RECORD
  adds exclusive TOGGLE (press a step to add/remove its event).
- Retains K, OptoCat/Warm, EQ, pad colours, Standard Piano/Scale, Note Repeat,
  exFAT and experimental FAT32. Browser Delete uses physical **ERASE**.

[Settings and testing](MOD_SETTINGS.md) · [Readiness](RELEASE_READINESS-r27f.md).
ARM/QEMU/UBSan and full native VM checks are distinct from device confirmation.
Other Leusha reports are not declared closed by these changes.


## Contents

1. [Start, exit and saving](#1-start-exit-and-saving)
2. [Interface](#2-interface)
3. [Typing names with the pads](#3-typing-names-with-the-pads)
4. [Orig Note, key and Lights](#4-orig-note-key-and-lights)
5. [Chords](#5-chords)
6. [Recording chords (Poly)](#6-recording-chords-poly)
7. [Steps: step sequencer on the pads](#7-steps-step-sequencer-on-the-pads)
8. [Events / Step Edit](#8-events--step-edit)
9. [Chop / 16 Levels event editor](#9-chop--16-levels-event-editor)
10. [Sequence and playback lighting](#10-sequence-and-playback-lighting)
11. [Browser, search and preview](#11-browser-search-and-preview)
12. [Batch Copy](#12-batch-copy)
13. [Batch Extract](#13-batch-extract)
14. [Stereo → Mono](#14-stereo--mono)
15. [Pad EQ](#15-pad-eq)
16. [Resampling](#16-resampling)
17. [Limits](#17-limits)

## 1. Start, exit and saving

| Feature | Status |
|---|---|
| Starts from SD through the stock autorun mechanism. The mod checks the SHA-256 of the stock app and of its own files and runs a patched copy of the app in RAM. Firmware and boot loader are never written | confirmed |
| Starts when the card is inserted into a running MPC, and when the MPC boots with the card inside | confirmed (r23a) |
| r27f shows Am MOD during preparation, then loads the SD project once. Public r26s still has the older dark-screen transition and Mod Off | r27f candidate; device timing varies |
| In r26y normal shutdown saves the recovery project and its samples to `MPC-Sample/Projects/Mod-Exit-Last.xpj` + `Mod-Exit-Last_[ProjectData]`. Mod Off is removed; copy failure keeps the app open with Back/Retry | implemented; new device check pending |
| **Power key** (short press): stock question → project copy to SD (*Saving project copy* screen) → power off | confirmed (r23) |
| Saving projects and samples to **External** (`MPC-Sample/Projects`, `MPC-Sample/Samples`, including subfolders); the stock app opens them | confirmed |
| **Internal** is temporary in the mod: anything saved there is lost when the mod stops | — |
| The browser names the storage after the folder actually open: SD or Internal RAM | confirmed |
| Mod settings (Orig Note, Lights, sequence colours and more) are kept in an extra field of the XPJ project. The stock app ignores it and drops it when it re-saves the project | confirmed (Orig Note, r23a) |

Startup selects the newest saved XPJ in SD Projects or one folder level below,
using modification time, rather than a history of manually opened projects.
The exit copy replaces the previous Mod-Exit-Last only after a successful save.
Unresolved project-transition/Save cases remain in [L01–L04](KNOWN_ISSUES.md).

## 2. Interface

| Feature | Status |
|---|---|
| **K1–K3 panel**: parameter name and value always visible, centred, on black. The selected field has an amber name, value, bracket and bar. Continuous parameters show a bar, switches show segments | confirmed |
| Readable labels instead of internal names: **Length**, **Q**, **Time Sig** | confirmed |
| Framed **S** marks next to parameters that change with Shift: white, red while Shift is held. For example, Sample → Mix shows **Kit Volume** on K2 with Shift | confirmed |
| Loop lock moved to the right edge of K3 | confirmed |
| **Top buttons and tabs**: one black background, centred labels, compact page indicators (amber for the selected one). Tab groups look different from single actions | trial (coverage of all screens) |
| Confirmation dialogs show the real answer labels instead of internal `Answer1 / Answer2` | confirmed |
| **Note Repeat** and **Knob FX**: amber top panel with black labels; Knob FX also has an amber K1–K3 background | trial |
| **Pan** and other ranges symmetric around zero (Tune) fill from a centre mark to the left or right | trial (r24q) |
| **Mod windows** (Chords, Steps, Steps edit menu, Stereo → Mono, Resample) share one block layout: B1–B3 blocks on top, information blocks, a 4×4 grid that mirrors the pads | trial (r24s) |

**Current r27f scheme.** Historical feature confirmations below do not cover
every newer combination. The full [mode map](PAD_LIGHTING_MAP.ru.md) also contains
proposals; the implemented subset is described here.

**Project → Mod Settings:** lighting, waveform/spectrum tint, service colours and cursor timing. B2/B3 switch tabs; Restore recommended is last and requires B3 confirmation. Settings are stored in XPJ; menu edits also set SD defaults. Standard keeps Piano/Scale and Transpose16. Mod Off is removed; normal-power recovery saving remains. Colours and timing below describe recommended defaults, configurable in the menu.

| Am MOD state | Indication |
|---|---|
| Sound/sequence identity | Default + Blue, Violet, Magenta, Orange, Pink; legacy IDs retained, reserved hues mapped for display |
| Current Sample/Chop pad or Steps Sound picker | Configurable cursor; Normal400ms white /400ms identity. Empty selected Sample pads turn off between flashes |
| Selected Copy/Batch Extract/Mono/Resample sources | White / assigned hue; Cursor pulse: Steady, Slow (1200ms), Normal (800ms), Fast (400ms) |
| Empty destination without assignment/selection | Off |
| Occupied pad outside selected targets | Dim identity hue |
| Assigned or current empty target | Green; current target also pulses in brightness |
| Assigned or current occupied target | Red; current target also pulses in brightness |
| Erase / overwrite | Red system cue; operation confirmation retained |
| Mute / muted recorded event | Dim grey / grey event flash, audio stays muted |
| Steps | Sound hue (Default blue), white cursor, cyan playhead, grey Mute; Standard uses blue, white, green playhead, red Mute |
| Seq in Am MOD | Selected once per bar at Stop, playing once per beat, queued twice per beat; background and other occupied pads stay visible |

System colours cannot be selected as new Pad Colour/Seq Colour assignments.
Musical Lights/Chords roles and the typing keyboard use separate palettes.
Dedicated new Loop/Song status markers remain proposals.

In Am MOD, set **Sample → B1 Mix → K2 Pad Colour without Shift**.
Encoder presses cycle Volume → Pad Colour → Pan. **K1 remains Volume**,
**Shift+K2 remains Kit Volume**. Standard removes the extra Pad Colour control.

## 3. Typing names with the pads

Works in Save Project / Save Sample and in search. **K1** switches Encoder / Pads and keeps the text typed so far. The basic keypad is confirmed on the device.

| Pad | Letters | Digits |
|---|---|---|
| 13 | `_` | 1 |
| 14 / 15 | ABC / DEF | 2 / 3 |
| 9 / 10 / 11 | GHI / JKL / MNO | 4 / 5 / 6 |
| 5 / 6 / 7 | PQRS / TUV / WXYZ | 7 / 8 / 9 |
| 2 | space | 0 |
| 1 / 3 | cursor left / right | |
| 4 | letters ↔ digits | |
| 8 | cycles `. - _ #` | |
| 12 | upper case while held | |
| 16 | Backspace (delete left); **Shift+16** is Delete (delete right) | |

- Pressing a pad again cycles its letters. Another pad, or a pause over 1 second, commits the letter.
- The encoder and +/− move the same cursor. The insertion point is shown as `|`, which never ends up in the name.
- Colours: letters white, digits green, symbols amber, case violet, delete red.
- Latin letters only.

**r27q:** in the name window (Save Project / Save Sample) **Erase** deletes in both input modes: before the cursor with pads, the selected letter with the encoder.

## 4. Orig Note, key and Lights

| Feature | Where | Status |
|---|---|---|
| **Orig Note**: the sample's original note (C…B or `--`). It only labels the sample and does not change the sound. Semi Tune shows semitones and the name of the resulting note | Sample → Tune → Shift+K1 | confirmed, including save/reopen (r23a) |
| Project **Key / Scale**: K1 tonic, K2 scale (Major, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Chromatic) | Project → Key / Scale | confirmed |
| **Lights**: pad colours for 16 Levels Tune by note | Sample → B2: Tune → Play → **Lights** | partly confirmed |

Lights: K1 is the **Layout**, K2 the tonic, K3 the scale; with Shift, K1 is Transpose16, K2 the colour group, K3 its colour.
- Layouts: **Monochrome** (stock colours), **Piano** (white and black keys), **Scale** (tonic / in scale / out of scale, every pad lit) and **Degrees** (r27q: the 16 pads play **only notes of the key**, one scale degree per pad. Pad by pad you go up the scale: with C Major and Orig Note C on the original pad you get C D E F G A B C D…; Transpose16 then shifts by scale degrees. If the original note is not in the key, the layout starts from the next key note above it and the original note is not shown. All pads are lit; the tonic I, IV and V are highlighted, each in its own colour, other degrees are dimmer). Default colours for new sounds: tonic Red, key notes Cyan (also in Scale), IV Violet, V Yellow, original note Magenta, out of key (Scale) Grey. In Chromatic, IV and V are the perfect fourth and fifth; in Lydian and Locrian they follow the scale (#4, b5). Older Tonic values load as Scale; projects saved with Degrees open as Scale in r27p and earlier.
- In Degrees, the Shift colour groups are **I Tonic**, **Key notes**, **IV**, **V** and **Original**; in Scale they are Tonic, Scale, Other and Original (Tonic and Scale/Key notes are shared). A changed IV or V colour is saved in project format MPCS19; r27p and earlier cannot read the mod settings of such a project. With default IV/V colours the project stays readable by older builds.
- Tonic and scale either follow the project (shown as **(P)**) or are set per sound. 12 colours including Off; the sample's original pitch has its own colour.
- **Transpose16 −24…+24** shifts the 16 Levels Tune layout without changing normal pad playback.
- Colours follow Orig Note, Semi Tune, pad number and Transpose16. Without an Orig Note the stock colours are used.
- Lights also apply in the Tune event editor from Events and Steps (see §9).

### Quick start: 16 Levels Piano/Scale

1. Select a sound; set **Sample → Tune → Shift+K1 → Orig Note**.
2. Set **Project → Key / Scale** if needed.
3. Press Sample **B2: Tune → Play → Lights**. K1 (Layout) chooses Piano, Scale or Degrees,
   K2 the tonic, K3 the scale. `(P)` means the project setting is inherited.
4. Enable **16 Levels**; choose **Tune** via **Shift+16 Levels → Type**.
5. On Lights, **Shift+K1 → Transpose16** shifts the layout. Lights work in
   Standard and Am MOD. Monochrome or no Orig Note leaves native colours.

Checked against the code; a complete physical-button walkthrough is still pending.

## 5. Chords

**Status: trial (r24v).** A chord mode on top of 16 Levels Tune.

1. Pick a melodic sample, set its **Orig Note**, and set Play to **Poly**.
2. The key comes from the project's Key / Scale or the sound's Lights.
3. Turn on **16 Levels → Tune** and press **Chop**. **B1 Back** or Chop again returns.

| Pads | Function | Colour |
|---|---|---|
| 1–8 | eight scale degrees (roots) | green |
| 9 / 10 / 11 / 12 | add 6 / 7 / maj7 / 9 | amber |
| 13 / 14 / 15 / 16 | Maj / Min / Dim / Aug; 13+14 = Sus4 | cyan |

- **K1** octave −2…+2. **K2** inversion −3…+3. **K3** arpeggiator: Off, Up, Down, Up-Down, Up-Down+, Random, As Played, Chord. **Shift+K3** arpeggio rate 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32; **Shift+K1** range 1–2 octaves. **B3** Latch, **B2** reset. Encoder click selects the parameter.
- The arpeggiator is **Off** on each entry; turned on, it starts at 1/4. It follows the project tempo and plays only while roots are held.
- **Note Repeat** held: the held chord repeats as a whole at the stock Note Repeat rate (1/16 until another rate is chosen).
- **Knob FX** works as usual: while it is on, K1–K3 control the stock knob effects.
- The screen mirrors the pads: chord name with additions, its notes, sample and key, then a 4×4 grid (types white, additions amber, roots green, last root outlined).
- Tap Tempo works inside Chords; SEQ, Sample and the other mode buttons leave it.
- With Latch On the chord type stays after release, and additions toggle on each press. With Latch Off they work while held.
- Seq Record + Play records every note of the chord as a separate normal Tune event, editable later in Events and Steps.
- Octave, inversion, Latch and arpeggiator reset on each entry and are not saved in the project.
- Stop pressed twice during playback releases every voice safely (fixed in r24f).

The type/addition layout was inspired by [ChoralRoot FM-1](https://github.com/Quixotic7/ChoralRootFM1); none of its code is used.

## 6. Recording chords (Poly)

| Feature | Status |
|---|---|
| In 16 Levels → Tune with Poly, pads pressed together are recorded as **separate events**, each with its own pitch, velocity and length, also when released in reverse order. The project format stays stock | confirmed (r23a) |
| Live release in Gate mode is shared: the sound stops when the last pad is released. Recorded lengths stay separate | limit |

## 7. Steps: step sequencer on the pads

**Enter:** Shift+SEQ → B2 Steps. **Back:** B2 Events. It edits the same events as the stock Step Edit. **Status:** basic operation confirmed.

- 16 steps of the selected sound on the pads, in time order 13–16 → 9–12 → 5–8 → 1–4.
- +/− change page, the encoder selects a step. Shift+encoder moves the selected sound's events along the grid, wrapping around the sequence.
- **K1 Sound**, **K2 Grid** (1/4…1/64, triplets with Shift+K2), **K3 Velocity**; Shift+K1 selects the bank.
- **B1 Sound + pad** selects a sound silently. While Sound is held, the selected pad alternates white and its assigned colour (r26r, included in r26s).
- **SEQ RECORD** toggles a local WRITE: when OFF a pad selects a step, when ON it adds a note to an empty step. Works while stopped and while playing.
- **Erase+pad** deletes the selected sound's events on that step as one Undo step.
- **B3 Edit**: Copy / Paste / Paste to Pad / Note Length. **Shift+B2**: Follow / Hold the playback page.
- **Mute**: hold it and press steps, or press it briefly to latch the mode. **Shift+Mute** unmutes the whole sound.
- **Shift+K3 Other Sounds** dims steps that hold events of other sounds. Encoder click hides or shows the cursor.
- **C / L** marks on a step light the Chop / 16 Levels button, which opens the event editor (§9). On an empty step with WRITE ON, 16 Levels opens note entry.
- Steps follows §2: selected sound hue, empty steps off, Other Sounds dim white; cursor/playhead/Mute depend on Mod Settings → Pads.
- The screen shows the same 16 steps as a 4×4 grid. Frame, fill, event data and step numbers follow the physical-pad order; encoder click hides/shows the selection cursor and frame.

## 8. Events / Step Edit

| Feature | Status |
|---|---|
| The encoder scrolls the view along the Q grid, wrapping around; **Shift+encoder** moves the selected event along Q and keeps its other values and micro-timing | confirmed |
| +/− select a row; **B3 Erase** (red caption) deletes that row. Erase alone does not delete it; **Erase+pad** keeps the stock sequence-wide deletion with confirmation | updated r26a |
| **Shift+B3**: Copy / Paste / Paste to Pad / Note Length. A copy keeps pitch, chop, velocity, length and modifiers | confirmed |
| **C / L / CL** marks on events with a slice or a 16 Levels modifier | confirmed |

## 9. Chop / 16 Levels event editor

When the selected event has a C or L mark, the Chop or 16 Levels button lights up and opens a choice window. Works from Events and from Steps; if a step holds several matching events, they are listed first.

| Feature | Status |
|---|---|
| Pads choose a variant (slice, note, value). **B3 Apply** applies, B1/B2 cancel. Nothing changes before Apply | confirmed (r16b) |
| Audition of the chosen variant; the original event is muted while editing, the rest keep playing | L confirmed (r16c), C trial |
| The original variant is amber, the chosen one white | confirmed |
| Levels types: Velocity, Tune, Filter, Layer, Attack, Decay, Probability, Ratchet, Slice, Articulation; Tune pages by octave | confirmed |
| **Several notes in Tune**: pads add and remove notes (up to 16); Apply replaces the event with separate events as one Undo step | trial (r21) |
| The sound's Lights colours in the Tune editor | trial |

## 10. Sequence and playback lighting

| Feature | Status |
|---|---|
| **Sequence colour**: Shift+K1 in Seq; saved with the project | confirmed (r23) |
| Am MOD: **Default + five colours** — Blue, Violet, Magenta, Orange, Pink; Standard uses native colours and disables Seq Colour | trial (r26u–x) |
| Seq pulses follow native tempo and meter: selected once per bar at Stop, playing once per beat, queued twice per beat until handoff. Assigned hues and background glow remain; other occupied pads keep the same brightness on Play. Empty slots are dark unless selected or queued | trial (r26q) |
| While recording is armed or running, the Seq screen stays and the pads keep the stock Sample layout | trial (r24o) |
| During playback the sounding pad blinks in Sample, notes in 16 Levels, slices in Chop | trial |
| Mute no longer switches 16 Levels lighting back to the normal bank | trial |

## 11. Browser, search and preview

**Search:** Sample Select / Open Project → **Shift+B3 Search**. **Status:** search in the current folder confirmed, the rest trial; the r27q window and controls still need a wider device check.

- **Query page:** type with the pads (or the encoder with K1 = Encoder). The first three matches appear live under the query with the count in the top right, so the list is often not needed. **Shift+Erase** clears the query; Erase deletes one character. The previous query is restored when Search is opened again in the same session.
- **K1** Input (Pads / Encoder), **K2 Search in**: **Folder** (the open folder only), **Library** (the whole library the open folder belongs to, default), **SD Card** (the card's MPC-Sample library), **All** (Library + SD card). The choice is kept until restart. **K3** preview volume. With Pads input the encoder moves the cursor; pad 16 deletes, Shift+pad 16 deletes forward. Pressing the encoder enters the letter: with Pads it confirms the multi-tap letter, with Encoder input it adds the next letter. B2 List / B3 Find open the results. With Encoder input, turning changes the letter, +/− move the cursor. **Erase** deletes in both modes (the selected letter with the encoder); Shift+Erase clears the query.
- **B2 List / B3 Find** open the list; in the list **B2 Query** returns to typing, **B1** closes Search.
- **List:** five rows; the selected row is highlighted and its folder is shown below. Case-insensitive. Samples: WAV, AIF/AIFF, MP3, FLAC, OGG. Projects: XPJ. Up to 4096 matches.
- Samples: tap a pad to choose the target (**Load to**), **press the encoder to load** and stay in the list. **B3 Preview** previews while scrolling; it is on by default. **Shift+press** opens the file's folder with the file selected. Pads follow §2: selected empty targets green, occupied targets red; unselected empty pads off, other occupied pads dimmed in their own hue.
- Projects: a press asks for confirmation (B3 Open / B1 Cancel).
- **K3 Volume 0–100 %**: preview level relative to Main Volume, shown on an audio file.
- **Erase** on an audio file in `Samples/User` runs the stock Delete with its stock confirmation.
- In **Open Project**, physical **Erase** deletes the selected XPJ with its
  companion `_[ProjectData]` after confirmation. For a project folder or a loose
  `_[ProjectData]` folder, **B1 Cancel / B3 Delete** confirms folder deletion,
  including its samples. This cannot be undone. L05 retains the reported
  browser/search state that still needs a device check.

## 12. Batch Copy

**Enter:** Sample → Shift+Erase → B2 Batch. **Status:** confirmed (r14e).

- Up to 16 source pads from any banks; a **Source → Destination** table.
- **K1** auto bank. **K2** auto or a specific pad. **K3** First free / Same pad / Bank free. Manual targets with Shift+K1/K2 or the pads; Erase sets a row back to Auto.
- **Shift+K3**: Sound only / **Sound + events**, which copies the sound's events in every sequence (trial, r15e).
- Occupied targets are marked `!`. B3 shows the number of copies and overwrites, B3 again confirms.
- Encoder click auditions the row's source. One Undo reverts the whole operation.
- Pad colours follow §2: selected sources follow the configurable white Cursor pulse, assigned empty targets green, occupied targets red; unselected empty pads off.

## 13. Batch Extract

**Enter:** normal Chop with a waveform and slices → hold Shift → **B2 Batch**. Shift+B1 is the stock Extract, Shift+B3 Split. Stock Merge (Shift+B3) is replaced: **Erase + pad** removes that slice and merges it with the previous one, the same result as Merge. **Status:** confirmed on the device (r15c: three samples with the correct frame counts).

- Pick 1–16 chops; a **C001 → B01** table with the same target rules as Batch Copy.
- Creates separate trimmed samples `Chop-A01-001`… and assigns them to the targets. One Undo; on error everything is rolled back.
- Encoder click auditions a chop, a second click stops it.

## 14. Stereo → Mono

**Enter:** Sample → **Shift+B3**. **Status:** trial (r21a/r21d).

- A pad picks the target, K1 or Bank the bank. **B1 Cancel**, **B2 Left**, **B3 Right**.
- The chosen channel of the whole sample becomes a separate mono sample with the source's settings. An occupied target needs a second confirmation. One Undo.
- The screen shows Source and Target blocks; the target turns red when it would overwrite a sound.

## 15. Pad EQ

**Enter:** Sample → B3: **Filter → Filter Env → EQ**. **Status:** audible result confirmed (r19c).

| Control | Function |
|---|---|
| K1 | Freq |
| K2 | Gain ±18 dB |
| Shift+K2 | Q |
| K3 | Bypass, Bell, Low Pass, High Pass, Notch, Low Shelf, High Shelf |
| encoder click | select parameter (reverse with Shift) |

The screen shows the frequency response. Low Pass, High Pass and Notch show `--` instead of Gain.

While the selected pad sounds, its **spectrum** (4096-point FFT, 20 Hz–20 kHz) appears on all Sample B3 pages: **Filter → Filter Env → EQ**. EQ draws dim amber bars behind its response curve; Filter and Filter Env suppress only the waveform, preserving their native filter/envelope curves and parameter knobs; the spectrum uses a dim translucent background. The spectrum falls off after the sound stops.

## 16. Resampling

**Enter:** Sample → **Shift+Pad 11**: B1 Cancel, **B2 Seq** (stock sequence resample), **B3 Sample** (the selected pad with its own processing).

| Feature | Status |
|---|---|
| Renders the selected pad with its settings and FX onto a chosen pad. Loop plays once for the render, the tail rings out. A silent or failed result is never assigned | confirmed (r20g) |
| Includes the source pad's direct Pad Links and the Bypass state | confirmed (r23a) |
| Target picker follows §2: selected empty green, occupied red, unselected empty off. Result name: "name Resample" | render confirmed; new lighting trial |
| The target page shows source/bank in a4×4 grid, using selection/colours from §2 | trial r26y |

## 17. Limits

- Firmware **1.3.0.12** only: the mod refuses to start on any other version.
- Internal is temporary in the mod; work on External.
- The stock app does not read the mod's project settings and drops them when it re-saves the project. The musical content of the project is kept.
- Automatic migration of name-keyed Lights settings on sample rename is not implemented. This is separate from implemented Pad Colour/EQ inheritance in Copy/Extract with Undo/Redo.
- An emulator run does not replace a hardware test: timing, audio, battery and USB audio differ.
- A fast arpeggio on a slow card can bring up the stock *Disk too slow* message.

Defaults Am MOD / Normal; saved card/project settings take priority. One editing cursor does not restrict simultaneous sample playback. Complete changes since r26s: [r27f](RELEASE_NOTES-r27f.md).

Defaults Am MOD / Normal; saved card/project settings take priority. One editing cursor does not restrict simultaneous sample playback. Complete changes since r26s: [r27f](RELEASE_NOTES-r27f.md).

## Rear recording and monitoring

- **Rear recording and monitoring.** Sample Record → B2 Routing: K1 chooses Record Rear L / Record Rear R / Record L + R. Press a pad to record mono. For L + R, choose the L pad, then a different R pad; recording starts simultaneously after the second choice. B1 or STOP cancels the first choice; occupied pads require confirmation. STOP finishes recording. Hold Pad Bank and use pads 1–8 for banks. Standard stereo recording on one pad remains in the normal Sample Record window; Routing is offered only for the Rear source.
- **Monitor without recording.** Input Config → Shift+B2 Routing: B2/B3 toggle L/R monitoring, K2/K3 set pan, Shift+K2/K3 set monitor level. The same controls work in recording Routing. Input Config pad presses never start recording. Monitor gain/pan do not change the dry mono recordings. Settings are saved with the project.
- **Knob FX priority.** Physical K1–K3 control Knob FX while it is open, including over custom screens. Routing resumes afterwards. Note Repeat keeps its controls.
- **Scale and Orig Note.** Setting Orig Note selects Scale. Tune lighting resolves the current sample from the project model, including when a different pad is selected in Compressor and across banks A–H.
- **Menu clicks.** A fast encoder click after leaving Mod Settings is passed to the current native menu. Hidden Sample controls no longer take Project controls.
- **Copy and EQ.** Hold Erase after entering Copy with Shift+Erase; Shift+Pad Bank switches banks backwards. Returning from Seq to Sample retains the EQ page.

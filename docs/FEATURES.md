# AM Mod features

**English** · [Русский](FEATURES.ru.md)

State as of build **r26a** (1 November 2026), MPC Sample firmware **1.3.0.12**.

**Statuses:**
- **Confirmed**: checked on a real MPC Sample.
- **Trial**: the specified automated checks passed; hardware coverage is pending as noted.

A confirmation covers that specific trial, not every combination of features in later builds. Build history: [CHANGELOG](../CHANGELOG.md).

Terms: **B1–B3** are the three buttons above the screen, **K1–K3** the three knobs below it, "encoder" is the large DATA wheel.

## r26a update — 1 November 2026

- **Pad Colour across sound views:** Sample and Events sound pads use their assigned colours; FX Select keeps its native status colours. Events names use each sound's colour. In Steps, ordinary events use the selected sound's Pad Colour (Stock keeps the standard green).
- **Steps background:** empty steps are off; events of other sounds have a very dim white background, enabled by default. **Shift+K3 → Other Sounds** switches it off/on. If other sounds play on every step, this background can cover all 16 pads of a page. The selected sound's own events take priority and keep their colour.
- **Cursor:** the on-screen frame, pad fill and step numbers now agree in the order **13–16 → 9–12 → 5–8 → 1–4**. Encoder click hides/shows the selection cursor and its frame. Selection, playback, Mute and Erase indications take priority over sound colours.
- **Events deletion:** **B3 Erase**, labelled red, deletes the selected event row. An Erase tap does not delete a row; **Erase+pad** keeps the stock sequence-wide deletion for that sound, with confirmation. Shift+B3 still opens Copy/Paste; B3 in the Chop/Levels picker still applies the selection.

Published at the author's request after device trials. The automated checks cover native LED conversion, both LED senders, all 16 screen positions on two pages, event controls, normal/UBSan ARM fixtures and preload construction. They are separate from hardware test coverage; see the release verification record.

## r25x update — 1 November 2026

The author confirmed the current features on an MPC Sample with r25w. r25x removes temporary diagnostic messages and prepares the public documentation; feature behaviour is unchanged. ARM/QEMU checks are listed in the release notes and do not establish coverage of every setting combination on hardware.

- **Pad Colour:** Sample → B1 → **Mix** → **K2 without Shift**. 13 choices including **Stock**, saved per pad in the project. Encoder presses cycle Volume → Pad Colour → Pan; turning changes the focused parameter. **Shift+K2 is Kit Volume** and K1 remains Volume. Mute and Erase keep their native status colours.
- **Erase in Open Project:** project files use the native confirmation and also remove the companion `_[ProjectData]` directory. A project folder or a loose `_[ProjectData]` folder has a separate question: **B1 Cancel, B3 Delete**. Its samples are deleted too; this cannot be undone.
- **One exit copy:** `MPC-Sample/Projects/Mod-Exit-Last.xpj` and `Mod-Exit-Last_[ProjectData]`. The previous copy is replaced after the new save succeeds; staging is not listed as another project.
- **Latest project at startup:** opens the newest `.xpj` on SD in Projects or one directory level below it. Normally this is Mod-Exit-Last; a project saved later in the stock app takes priority. Selection uses file modification time, not a history of manually opened projects.
- **Storage labels:** browser B2 shows **Internal / External** from the actual path. Internal remains temporary in the mod.
- **Lights:** three modes — **Monochrome, Piano, Scale**. Older Tonic/Degrees settings load as Scale. Tonic, scale, Transpose16 and group colours remain available.
- **Interface:** framed **S** badges, white normally and red with Shift; Zoom: Loop lock at the right edge; a Seq Colour swatch. Pad-name entry has cursor and repaint fixes.

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
| On start the stock app closes: the screen and the pads go dark for a few seconds, then the mod appears. The same on Mod Off. Do not power off or pull the card meanwhile | expected behaviour |
| **Project → Mod Off**: the last item of the Project menu (Shift+Pad 16), with a confirmation: B1 Cancel, B3 Mod Off | confirmed |
| On Mod Off the mod saves a separate copy of the project with its samples to `MPC-Sample/Projects/Mod-Exit-Last.xpj` + `Mod-Exit-Last_[ProjectData]`, then the stock app returns. If saving fails, the mod stays on with Back and Retry | confirmed |
| **Power key** (short press): stock question → project copy to SD (*Saving project copy* screen) → power off | confirmed (r23) |
| Saving projects and samples to **External** (`MPC-Sample/Projects`, `MPC-Sample/Samples`, including subfolders); the stock app opens them | confirmed |
| **Internal** is temporary in the mod: anything saved there is lost when the mod stops | — |
| The browser names the storage after the folder actually open: SD or Internal RAM | confirmed |
| Mod settings (Orig Note, Lights, sequence colours and more) are kept in an extra field of the XPJ project. The stock app ignores it and drops it when it re-saves the project | confirmed (Orig Note, r23a) |

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

**Pad colours in mod windows** (Steps, event editor, search, Batch Copy / Extract, Stereo → Mono, Resample). Musical colours (Lights, Chords, sequences) are separate.

| Colour | Meaning |
|---|---|
| green | has a sound or events; the source of a copy |
| white | cursor, chosen value, target |
| dim white | has a sound, but is not part of the operation |
| cyan | playback position only |
| amber | the original value being replaced |
| red, blinking | will be erased or overwritten |
| violet | muted |
| dim grey / off | empty |

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

## 4. Orig Note, key and Lights

| Feature | Where | Status |
|---|---|---|
| **Orig Note**: the sample's original note (C…B or `--`). It only labels the sample and does not change the sound. Semi Tune shows semitones and the name of the resulting note | Sample → Tune → Shift+K1 | confirmed, including save/reopen (r23a) |
| Project **Key / Scale**: K1 tonic, K2 scale (Major, Natural Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Chromatic) | Project → Key / Scale | confirmed |
| **Lights**: pad colours for 16 Levels Tune by note | Sample → B2: Tune → Play → **Lights** | partly confirmed |

Lights: K1 is the mode, K2 the tonic, K3 the scale; with Shift, K1 is Transpose16, K2 the colour group, K3 its colour.
- Modes: **Monochrome** (stock colours), **Piano** (white and black keys), **Scale** (tonic / in scale / out of scale). Older Tonic/Degrees values load as Scale.
- Tonic and scale either follow the project (shown as **(P)**) or are set per sound. 12 colours including Off; the sample's original pitch has its own colour.
- **Transpose16 −24…+24** shifts the 16 Levels Tune layout without changing normal pad playback.
- Colours follow Orig Note, Semi Tune, pad number and Transpose16. Without an Orig Note the stock colours are used.
- Lights also apply in the Tune event editor from Events and Steps (see §9).

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
- **B1 Sound + pad** selects a sound silently.
- **SEQ RECORD** toggles a local WRITE: when OFF a pad selects a step, when ON it adds a note to an empty step. Works while stopped and while playing.
- **Erase+pad** deletes the selected sound's events on that step as one Undo step.
- **B3 Edit**: Copy / Paste / Paste to Pad / Note Length. **Shift+B2**: Follow / Hold the playback page.
- **Mute**: hold it and press steps, or press it briefly to latch the mode. **Shift+Mute** unmutes the whole sound.
- **Shift+K3 Other Sounds** dims steps that hold events of other sounds. Encoder click hides or shows the cursor.
- **C / L** marks on a step light the Chop / 16 Levels button, which opens the event editor (§9). On an empty step with WRITE ON, 16 Levels opens note entry.
- Ordinary events use the selected sound's **Pad Colour** (Stock: green). Empty steps are off. Other Sounds is dim white; selection is white, playhead cyan, erase red/blinking and muted steps violet.
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
| 13 colours: **Stock** (the stock pad hue, default), Green, Amber, Violet, Magenta, Red, Orange, Yellow, Blue, Lime, Mint, White, Cyan | trial (r24v) |
| Blinking follows the tempo: the playing sequence once per beat, the queued one (pressed during playback, starts at the end of the bar) twice per beat. Empty sequences are dark, an empty queued one still blinks | trial (r24p) |
| While recording is armed or running, the Seq screen stays and the pads keep the stock Sample layout | trial (r24o) |
| During playback the sounding pad blinks in Sample, notes in 16 Levels, slices in Chop | trial |
| Mute no longer switches 16 Levels lighting back to the normal bank | trial |

## 11. Browser, search and preview

**Search:** Sample Select / Open Project → **Shift+B3 Search**. Type the query with pads, the encoder and +/−. B2 switches between query and results, B1 closes. **Status:** search in the current folder confirmed, the rest trial.

- Scope: **Folder / Subfolders / Library** and storage (Current / Internal / SD Card), on K3 or Shift+K3.
- Case-insensitive. Samples: WAV, AIF/AIFF, MP3, FLAC, OGG. Projects: XPJ. Up to 4096 matches.
- Samples: a pad silently picks the target, **encoder click loads** the sample and stays in the results. **B3 Preview** turns on preview while scrolling. **Shift+click** opens the file's folder with the file selected. Pads use the shared palette (§2): the target white, red and blinking when it would replace a sound; occupied pads dim white (r25x).
- Projects: a click asks for confirmation (B3 Open / B1 Cancel).
- **K3 Preview Vol 0–100 %**: preview level relative to Main Volume, shown only on an audio file.
- **Erase** on an audio file in `Samples/User` runs the stock Delete with its stock confirmation.

## 12. Batch Copy

**Enter:** Sample → Shift+Erase → B2 Batch. **Status:** confirmed (r14e).

- Up to 16 source pads from any banks; a **Source → Destination** table.
- **K1** auto bank. **K2** auto or a specific pad. **K3** First free / Same pad / Bank free. Manual targets with Shift+K1/K2 or the pads; Erase sets a row back to Auto.
- **Shift+K3**: Sound only / **Sound + events**, which copies the sound's events in every sequence (trial, r15e).
- Occupied targets are marked `!`. B3 shows the number of copies and overwrites, B3 again confirms.
- Encoder click auditions the row's source. One Undo reverts the whole operation.
- Ordinary events use the selected sound's **Pad Colour** (Stock: green). Empty steps are off. Other Sounds is dim white; selection is white, playhead cyan, erase red/blinking and muted steps violet.

## 13. Batch Extract

**Enter:** normal Chop with a waveform and slices → hold Shift → **B2 Batch**. Shift+B1 is the stock Extract, Shift+B3 Split. **Status:** confirmed on the device (r15c: three samples with the correct frame counts).

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
| Target picker: the target pad is white (red and blinking when it would overwrite a sound), the source green, occupied pads dim white. The result is named "name Resample" | confirmed (colours updated in r24d) |
| The target page shows the source and the bank as a 4×4 grid: occupied pads grey, the source outlined green, the chosen target white, or red when it would overwrite | trial (r24t) |

## 17. Limits

- Firmware **1.3.0.12** only: the mod refuses to start on any other version.
- Internal is temporary in the mod; work on External.
- The stock app does not read the mod's project settings and drops them when it re-saves the project. The musical content of the project is kept.
- Mod settings do not yet follow a sample when it is renamed or copied.
- An emulator run does not replace a hardware test: timing, audio, battery and USB audio differ.
- A fast arpeggio on a slow card can bring up the stock *Disk too slow* message.

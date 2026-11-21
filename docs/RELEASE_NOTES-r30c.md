# Am MOD r30c — MIDI, projects, synth and Piano Roll

For **Akai MPC Sample stock firmware 1.3.0.12**. This release continues r30b. Installed firmware is not rewritten. Items marked *pending* have passed host/emulator checks but still need a hardware check on your MPC.

## MIDI

- **MIDI Input modes** (B3 cycles): Drum map, Channels, Selected pad and Instrument. Switching mode keeps your channel assignments; Shift+B3 resets the mode settings only.
- **Channels:** each MIDI channel gets a pad and a role: All pads, Tune, Chop or Synth. Tune and Chop recording from a MIDI keyboard is confirmed on MPC.
- **Selected pad:** K1 sets the incoming channel (All or 1–16), K3 the root note; the keyboard plays Tune/16 Levels on the selected pad.
- **MIDI Tools → MIDI Buttons:** a list of controller buttons and faders mapped to MPC buttons and knobs. B3 assigns (or press the MPC control), B2 deletes the highlighted entry or cancels, B1 goes back. Faders can be assigned; Shift+button combinations are supported.
- **Input Config → B3 MIDI In** opens the same MIDI Input settings (B2 Map/Learn, B1 back).

## Projects and storage

- Internal user folders (Projects, Samples/User) are saved permanently. Internal audio exports stay on Internal flash.
- Recovery remembers the original storage (Internal or SD) and the project's folder. The first Save returns to that place and never falls back to a same-named project on another volume. If the origin is not known, the screen says **“Original location unknown; save a new copy”**.
- At start-up the newest project among the card, Internal and the temporary autosave is loaded, whichever firmware (Stock, SampQOL or Am MOD) saved it. The project's audio data is staged so the native recorder cannot return an older sample with the same name.

## Sample Record and Synth

- **B2 To Seq:** a Mic take inserts the target pad's note into the playing sequence when the take starts. If playback is stopped the input waits until Play.
- **Synth:** 8 voices (sine, saw, square, triangle), ADSR and low-pass. Played from MIDI (Instrument mode or Synth role) or from the pads. Parameters: Mixer pad 13 → K1–K3 pages (Tone, Envelope, Keys).
- **Input routing** (Mixer → Input → ROUTE → Input): *Clean* (signal straight to the pads), *Track* (through the Input track and its FX, default) and *Synth* (the synth is the Input track source; *Mic* layers the microphone on top). *Direct out* adds the processed synth to the output when monitoring is off.
- **16 Levels for the synth** uses the same engine and colours as the project's 16 Levels: Monochrome, Piano, Scale and Degrees, with the project key and scale by default. Transpose moves the notes along the scale (steps in Degrees, semitones elsewhere); the key stays and the pad lights follow the notes. In Sample Record the keyboard starts off and the 16 Levels button turns it on (the button is lit while it is on).
- Pad Bank + knob resets a synth parameter to its default. Pad Bank letters A–H stay off on the synth page, as for Master and Input.

## Piano Roll, Song and Mixer

- Piano Roll shows the K1–K3 values above the knobs (scroll, zoom, move, length, velocity, grid).
- Mute on the selected row toggles that pad's local mute in the sequence; a red M marks muted rows, red SM appears in Piano Roll and Mixer headers.
- Song window: repeat table with step colours; Mixer pad lights follow the selected track.

## Limits

- The synth, the new Input routing and the MIDI Buttons page were developed after the last full device check and are *pending* hardware verification.
- Saving over an existing name does not yet show a warning in the native Save dialog.
- Opening a project that was last saved by another mod can report an unknown original location; save a new copy.
- Cross-mod Load/Save of Internal projects (Am MOD, Stock, SampQOL) is not fully certified.

See also [Known limitations](KNOWN_ISSUES.md) and the [changelog](../CHANGELOG.md).

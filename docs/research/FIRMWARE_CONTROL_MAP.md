# MPC Sample 1.3.0 — preliminary AC50 control map

Source: static analysis of `extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample`.
Offsets below are file offsets of printable strings in the ARM64 ELF. This is
evidence about named components and their relationships, not yet a complete
reconstruction of every branch in machine code.

## Confirmed AC50 components

- `AC50ControlSurfaceBehaviour` (`0x049889c8`) owns device-level button
  behaviour. Named handlers include `OnMinusButton`, `OnPlusButton`, and
  `OnSampleRecordButton` (`0x04988800`–`0x04988870`).
- `AC50StepEditController` (`0x049daf00`) owns Step Edit state. Its `StepInfo`
  value is observed by both `AC50TransportControlsResponder` (`0x0498ab70`)
  and `StepEditScreen` (`0x04a14ff0`).
- `AC50StepEditFaderResponder` (`0x0498a7c8`) proves that the physical fader
  has a separate Step Edit mapping.
- The AC50 fader menu strings at `0x045a2020`–`0x045a20a0` enumerate the stock
  mappings in order: `Pad Amp`, `Pad Volume`, `Pad Pan`, `Pad Tune`,
  `Pad Amp Attack`, `Pad Amp Decay`, `Pad Filter Cutoff`, and `Kit Volume`.
- `AC50ControlSurfaceLighting` listeners (`0x04989560`–`0x04989878`) react to
  mode, boolean state, pad/index changes, play direction, zone trigger mode,
  slice loop mode, and 16 Levels type.
- General pad-light resolution is layered through `PadLighting`,
  `PadLightingChain`, and `PadLightingChain::Refresh` (`0x049d79b0`–
  `0x049d7a28`). The final light value uses `Controls::PadLight` and is sent by
  `AC50MidiSender` (`0x049e37d0`–`0x049e37f8`).
- Pad banks are handled by `PadBankControls::OnPadBankButton`
  (`0x0498b120`). The firmware also exposes `BanksButtons.PressToSwitchBank`
  (`0x045ab2a8`) and four logical bank-button names (`0x04599f00`–
  `0x04599f60`).
- 16 Levels keeps both `SixteenLevelsType` and `SixteenLevelsOriginalPad`
  (`0x045b9738`–`0x045b9750`), with dedicated `SixteenLevelLighting` and an
  AC50 lighting listener.
- Pad colour behaviour includes `padsFollowTrackColour`, `PadColorGroupMap`,
  `PadMuteUsePadColours`, and custom/all-pad colour settings.
- Step-sequencer state includes current bar, selected pad, current step,
  playback-step count, tie, legato, rest, clear, lock, transpose, clock source,
  step speed, and swing (`0x046117d0`, `0x04635c08`–`0x04637040`).
- `SampleModeScreen` (`0x04a08088`) has three rotary callbacks and multiple
  parameter/state connections. Nearby AC50 UI strings identify `Trim`,
  `Amp Env`, `Tune`, `Zoom: Start`, `Zoom: End`, `Zoom: Loop`, `Pan`,
  `Fine Tune`, `Reso`, and `Flt Env` (`0x045a27d8`–`0x045a29f0`).

## Input and display model

The AC50-specific binary structure supports a hardware-driven model:

1. Physical controls enter `AC50ControlSurfaceBehaviour` and mode-specific
   responders.
2. Controllers update sequencer/sample state.
3. `AC50ControlSurfaceLighting` and `PadLightingChain` derive LED values from
   that state.
4. `StepEditScreen` observes `AC50StepEditController::StepInfo` and renders it.

The binary contains many `Touch*` classes because it shares a large MPC/JUCE
codebase with touchscreen products. Their presence alone does **not** show
that MPC Sample's AC50 display is touch-enabled. The AC50-specific links above
instead show the display as a listener to hardware-controlled state.

## Still requiring disassembly or a device trace

- Numeric hardware event IDs for every printed button.
- Exact short-press, hold, release, and Shift precedence for every mode.
- RGB/brightness constants for each pad-light state.
- The exact bank/page formula used by stock Step Edit.
- Menu cursor wrap/clamp rules and encoder acceleration.

These can be recovered next by following AArch64 references to the named AC50
classes and, where static analysis is ambiguous, logging the existing Lua and
control-surface state on the real unit with the reversible test-app path.

## Behavioural audit implemented in the virtual model

Sources are the 1.3.0 ARM64 ELF and the complete 67-page version 1.3 user
guide. The guide supplies user-visible ranges and gestures; ELF strings and
AC50 classes corroborate the screen names, control-surface ownership, fader
targets, pad-light layers, 16 Levels state, and Step Edit controller.

### Sample Mode

- B1 cycles `Trim -> Mix -> Amp Env`; Chop replaces Trim while active.
- B2 cycles `Tune -> Play`.
- B3 cycles `Filter -> Filt Env`.
- The selected B group owns K1-K3. K knobs do not invoke B buttons.
- Trim: Start, End, Loop; Shift+K1-K3 are the three waveform zoom controls.
- Mix: Volume, Shift+K2 Kit Volume, Pan.
- Amp Env: Attack, Decay/Release, Velocity Sensitivity.
- Tune: Semi Tune, Fine Tune, Warp; Shift+K3 is sample beat count.
- Play: Mono/Poly, Mute Group / Shift Pad Link, Offset.
- Filter: Cutoff, Resonance, Type with Off, Classic, LPF2/4, HPF2/4, BPF2/4.
- Filt Env: Attack, Decay/Release, Depth.
- Shift+B1 toggles Loop Lock; Shift+B2 normalizes the selected sample.
- Encoder selects K parameters; Shift+Encoder moves backwards.

### Pad Play and sampling

- Chop, Shift+Chop Note On, Loop, Shift+Loop Reverse, Mute,
  Shift+Mute Unmute All, and 16 Levels are represented as independent states.
- Stock 16 Levels types are Velocity, Filter, and Tune. The custom Pad Layout
  menu adds Chromatic and Scale without making the display touch-sensitive.
- Sample Browser B1/B2/B3 roles, Sample Record Monitor/Source/Length/Threshold,
  sample recall, destructive trim, normalize, and sequence resampling are
  mapped.

### Sequence and Step Edit

- Sequence B1 BPM edit, B2 SEQ/GBL tempo, B3 Record Quantize.
- K1 length, K2 Q, K3 real-time swing; Shift variants are time signature and
  metronome volume.
- Pads select/queue sequences and use dim, flashing, and solid green states.
- Step Edit uses encoder step navigation, +/- event navigation, fader timing,
  K1 length, K2 Q, K3 velocity, and B3 erase.
- The custom pad step page keeps 16-step pages and uses Pad Bank to reach
  steps 17-32 and later pages.

### Effects and menus

- Pad FX includes all 16 firmware-listed slots and their K parameter labels;
  B1 controls Latch.
- Flex Beat includes 16 slots, One Shot/Loop, Mix, and B3 Quantize.
- Knob FX selection includes the full guide list, pad targeting, B2 All Pads,
  and B3 Bypass.
- Compressor implements B1 Color, B3 Bypass, Attack, Release, Amount, and
  Shift+K3 Input Boost.
- Fader, Input Configuration, Time Correct, MIDI Configuration, Song, Project,
  file selection, and Sample Record have dedicated non-touch screens.

### Shift+Pad map

1. Full Level
2. Half Sequence
3. Double Sequence
4. Count-In
5. Compressor
6. Half Speed
7. Double Speed
8. MIDI Configuration
9. Fader
10. Record Quantize
11. Resample
12. Song
13. Trim Sample
14. Time Correct
15. Warp mode
16. Project

### Accuracy boundary

The virtual model now follows the documented control transitions and parameter
ranges, but it is not yet a bit-exact ARM execution of `/usr/bin/MPC Sample`.
Exact RGB values, button hold timing, soft-takeover curves, all DSP algorithms,
and a few hardware-only confirmation flows still require a real-device trace
or running the stock binary against emulated AC50 drivers.

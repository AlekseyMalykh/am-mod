# r27e — Mod Settings, project recovery and pad selection

Prepared **3 November 2026** for a **prerelease**. The public stable
release remains [r26s](https://github.com/AlekseyMalykh/am-mod/releases/tag/r26s).
This is the complete change list since r26s; intermediate candidate notes are history.

## Installation and update

- MPC Sample firmware **1.3.0.12**. **exFAT supported; FAT32 experimental**,
  with physical FAT32 confirmation still pending.
- **Back up samples and projects before installing.** Formatting is unnecessary.
- Replace the complete **autorun** folder at the SD root: `SD:/autorun/`.
  Keep `MPC-Sample/` and other user folders. The name is autorun, not autoload.
- Installer **1.0.2** is unchanged. Select this release's local ZIP, or copy
  autorun manually. While r27e is a prerelease, the installer's Latest action
  still selects the current stable release. Do not mix files from different ZIPs.

## New settings and controls

**Project → Mod Settings** replaces Pad Lighting. B2/B3 cycle five tabs;
B1 returns. Turn the encoder to select a row, click to edit, turn to change,
then click to finish. Lists scroll with selection.

| Tab | Controls |
| --- | --- |
| Pads | Standard / AM Mod |
| Wave / Spectrum | Independent sample waveform and spectrum tint |
| System Colours | Cursor, free target, occupied target, original value, playhead, muted |
| Selected Pad | Steady / Slow / Normal / Fast cursor |
| About, last | Version and repository address, read-only |

The four editable tabs end with **Restore recommended**. It opens a separate
confirmation: **B1 Cancel / B3 Restore**. About has no reset or editable values.
Service-colour changes swap palette owners, keeping five user slots and Default
separate from service roles. Defaults are Blue/Violet/Magenta/Orange/Pink for
user identity and White/Green/Red/Amber/Cyan/Grey for the six service roles.

**Initial defaults: AM Mod + Normal.** Normal alternates400 ms white/400 ms base;
Slow has a1200 ms cycle, Fast400 ms, Steady stays selected. Saved card preferences
and loaded-project snapshots take priority over defaults; an update does not
force them back. Standard disables user Pad/Seq Colour overrides while retaining
16 Levels layouts, Piano/Scale musical lighting and Transpose16 controls.

**Hold Pad Bank + turn K1/K2/K3** resets that control to its factory default.
For a Shift control, hold Pad Bank, then hold Shift and turn its K. Further
turns are consumed until Pad Bank is released. Native held-Pad-Bank selection
with pads1–8 and the ordinary bank button remain available.
Reset covers Sample, EQ, OptoCat, Lights, Chords, Steps settings and Key/Scale.
Sound/bank object selection, menus, Main Volume and native Duck/other FX are
not reset. This is a factory default, not the value from the last project load.

## Pad selection and colour fixes

- A shared configurable white cursor distinguishes the working pad and selected
  Copy/Batch/Chop Extract/Mono/Render sources from assigned sample colours.
- Sample now marks **empty selected pads**, including an empty program. They
  turn off between flashes; occupied selected pads reveal their assigned hue.
  Other empty pads stay off. Selection restarts the cursor phase immediately.
- The sample browser cursor follows the **actual current load target**, including
  empty pads and all eight banks. It no longer stays on the pad selected before
  entering the browser. Search initializes Load to from that same target.
- Unselected empty destinations stay off. Assigned/current empty targets use
  green, occupied targets red. When source and destination overlap, flashing
  alternates selection with destination status; Steady gives destination status
  priority. Palette swaps can change these service hues.
- Sample/Copy/browser/Mono/Render/Extract retain pad identities. Muted events
  can still flash grey without unmuting audio. In Standard, native lighting
  keeps priority. The configured cursor also applies to the held Sound picker.
- Several samples can play together. **One cursor marks the editing selection**,
  normally the last processed pad press; the other sounding pads retain their
  identity/activity indication. Repeated presses on the same pad do not restart
  the selection phase or enable a special double/triple-tap action.

## Projects, startup and shutdown

- Display settings, palette, tint, cursor speed and OptoCat Type/Threshold/Ratio/
  Gain/Attack/Release/Knee/Warm are stored in an optional XPJ snapshot.
  Native Duck and shared Bypass serialization remain intact.
- Successful Load restores the snapshot; failed Load leaves the session settings
  intact. Legacy projects keep display preferences and reset OptoCat safely to
  Duck defaults. New Project resets OptoCat but retains display preferences.
- Startup chooses the newest saved SD XPJ in Projects or one subfolder and
  substitutes it into the **first complete native Load**. Late duplicate loading
  is disabled. This is modification-time selection, not a last-opened history.
- Normal shutdown saves a complete recovery copy with samples. Recovery restores
  the original display name/path while retaining recovery sample paths and the
  native Temp-Project autosave destination. First Save after recovery requires
  a separate confirmation; it does not silently overwrite the original project.
- Fixed the missing Temp-Project file path responsible for delayed **Auto Save
  Error** after New Project: the first necessary autosave initializes it using
  native serialization of the current project. Existing files, external paths
  and genuine write failures retain native handling.
- **Project → Mod Off is removed.** Save to External and power off normally;
  start without the mod card to use stock firmware. Internal storage in the mod
  is temporary RAM storage, so keep projects and samples on External.
- A minimal **Am MOD** DRM splash covers application preparation. Version and
  repository text are small and near the bottom; About repeats them. There is
  no artificial minimum display time. A device log measured 17 s of preparation;
  boot time still depends on the card/project and is not guaranteed faster.

## Steps, browser and display corrections

- **SEQ RECORD = WRITE**, **SAMPLE RECORD = TOGGLE** in Steps. TOGGLE adds/removes
  the selected sound's events at a step. These modes are mutually exclusive:
  enabling either turns the other off. Other sounds' events remain intact.
- Browser Delete uses the physical **ERASE** button and native confirmation.
- Note Repeat B1–B3 take priority over compressor Type/Warm controls. Hidden
  overlay paints no longer re-open the Type indication after Repeat closes.
- Optional waveform/spectrum tint follows pad colour. Filter/Filter Env retain
  spectrum without waveform, preserve native response/envelope curves, and EQ
  keeps native Volume/Pan side strips. Tint does not recolour the controls.

## Included from the public r26s release

This release preserves the device-confirmed **Scaled/K1–K3 end-stop fix**,
always-visible K values and Shift badges; pad-name entry; Orig Note and Key/Scale;
Piano/Scale and Transpose16; Steps/Events with polyphonic notes and audition;
sample/project search and preview; Batch Copy/Extract with colour Undo/Redo;
sample/sequence resampling and Pad Link; stereo-to-mono; per-pad EQ and spectrum;
tempo-synced Seq pulses; compressor colours and normal-power recovery saving.

**OptoCat/Warm remains included:** Gain−6…+6dB (default0), Shift Attack/Release/
Knee, Warm, sleep/background suppression, input-driven bow and the paw driven by
the same real gain reduction as the red bar. Duck/OptoCat are alternative
algorithms with shared Bypass; Shift+Pad5 opens the active one, or Duck when both
are off. No compressor DSP or gain limit is changed by the final cursor fixes.

## Validation and remaining work

ARM/QEMU and UBSan cover source/target roles, all 128 selection indexes/eight banks,
four cursor modes, empty/no-program cases, native browser target setter, Steps
WRITE/TOGGLE, Note Repeat routing, project/settings recovery, missing-file autosave,
native K handling/reset, EQ/filter drawing and OptoCat DSP. The complete preload
constructor installs all 50 callbacks. The prepared payload is byte-identical to
the r27e build written/read back on the author's card:75 files, six XPJs and user
files unchanged at that write. Documentation refresh changes no autorun payload.

Earlier native VM tests and device feedback verify specific subsystems; **a full
VM boot or physical pass of every feature in this exact final package is not
claimed**. Final browser/empty-Sample cursor visuals still need device confirmation.
The K end-stop fix and basic project/splash checks have earlier device confirmation.

Still open or requiring the original hardware scenario: project lifecycle reports
L01–L04, first-pass Chop light L06, muted waveform L11, native Copy reverse-bank L13,
full colour/mute/compressor workflows, fast/latched Note Repeat and physical FAT32.
Implemented does not automatically mean the complete tester report is closed.

Not included: a synth (deferred), stem export, a mixer/effect-routing system,
an independent RGB editor and the other unscheduled requests in
[Known issues and requests](KNOWN_ISSUES.md).

Ideas C01/C02: live held-chord/arp changes and force-based expression are [future requests](IDEAS.md), not implemented in r27e. New-hit velocity already passes through; continuous pressure still needs investigation.
Plan M01: audit MIDI In/Out, channels/clock, external Chords/arp operation, velocity and Note On/Off. Details are in the same roadmap; verification is pending.

Thanks to **Leusha**, **mxt1iga** and everyone who provided tests, reproductions
and feedback. [Full feature/control guide](FEATURES.md) · [Mod Settings](MOD_SETTINGS.md) ·
[Release readiness](RELEASE_READINESS-r27e.md).

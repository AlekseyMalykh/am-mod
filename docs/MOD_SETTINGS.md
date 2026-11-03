# Mod Settings — r27f

Available in public prerelease r27f; stable remains r26s.

**Project → Mod Settings** replaces Pad Lighting. **B2/B3** cycle tabs,
**B1** returns. Turn the encoder to select a row, click to edit, turn to
change its value, then click to finish. K1 can also change the selected value.
Lists scroll with the cursor.

| Tab | Controls | Recommended defaults |
| --- | --- | --- |
| Pads | Standard / AM Mod | AM Mod |
| Wave / Spectrum | Independent waveform/spectrum pad-colour tint | Original / Original |
| System Colours | Cursor, free/occupied target, original value, play position, muted | White, Green, Red, Amber, Cyan, Grey |
| Selected Pad | Steady / Slow / Normal / Fast | Normal: 800 ms cycle |
| About (5/5) | Version and repository URL; read-only | No settings or reset |

**Restore recommended is the last row in each of the four settings tabs.**
About is the fifth and last page; knobs and encoder clicks do not edit or reset anything.
B3 wraps About → Pads; B2 wraps Pads → About. Click to review the reset:
**B1 Cancel** keeps the settings, **B3 Restore** resets the displayed section.
Knob movement and another encoder click do not confirm.
The palette reset restores all user and service slots together.
Samples, events, pad assignments and compressor settings are preserved.

Changing a service colour swaps it with its existing owner. Giving Pink to
the white cursor makes the former Pink user slot White. Names update accordingly.
Five user slots plus Default remain, without collisions with six service roles.
Existing project colour IDs stay stable; swapping a slot changes every pad or
sequence assigned to that slot.

Standard disables Pad Colour/Seq Colour without deleting assignments and keeps
16 Levels Piano/Scale layouts. Optional waveform/spectrum tint applies in AM Mod.
Filter/Filter Env still show spectrum without waveform; native response curves,
envelopes and side strips remain intact. **Selected Pad → Cursor pulse** also
controls Copy/Batch/Chop/Mono/Render source selection and the library load pad.
**Steady** keeps the cursor white; **Slow / Normal / Fast** alternate white
with the assigned colour over 1200 / 800 / 400 ms cycles. Empty selected pads
turn off between flashes. If a source is also the chosen destination, white
alternates with red/green; in Steady, the red/green destination warning takes
priority. System Colours changes the cursor colour. Seq tempo pulses and
destination warnings keep their own timing.

Projects carry an optional **mpcSampleModSettingsV1** field with the display
mode, palette, tint, cursor timing, compressor Type and OptoCat Threshold, Ratio,
Gain, Attack, Release, Knee and Warm. The native serializer retains Duck controls
and shared Bypass. Settings commit only after a successful project read.
Legacy projects retain the current display preferences and reset OptoCat to Duck
defaults; native project data determines Bypass. New Project resets Opto controls
and retains display preferences.

Menu edits also set card defaults in `MPC-Sample/Projects/.am-mod-settings`;
loading a project does not overwrite those defaults. The previous lighting-only
file is migrated when the new file is absent. Without a writable SD, settings
are session-only and write failures are reported. Stock firmware may discard
mod fields when resaving: back up samples and projects before testing.

Test two projects with different display and compressor settings, load both
ways, then power-cycle. Check actual samples, notes and tempo as well as UI
values. Check tint, native filter/envelope visibility, Standard mode, all reset
rows and palette swaps on the device.

This candidate also removes the Project Mod Off row while keeping normal-power
recovery-copy saving and emergency stock recovery. Restored copies retain their
original identity and require a separate first-Save confirmation. A minimal
**Am MOD** splash is shown by the loader before application preparation.
The SD project is selected in the first native Load; late duplicate autoload
is disabled. Newest-saved-XPJ selection remains; r27a splash visibility is confirmed on MPC. The r27e metadata needs a device check. In Steps, SEQ RECORD keeps WRITE, SAMPLE RECORD
adds exclusive TOGGLE mode. Browser Delete uses physical **ERASE**.


![Am MOD r27e](assets/splash-r27e.png)

Updated 320×240 splash preview: smaller version and repository text near the bottom edge.

Initial defaults are AM Mod and Normal (400ms white /400ms pause). Empty selected Sample pads are fully off during the pause; occupied pads reveal their assigned colour. Saved card/project preferences take priority.

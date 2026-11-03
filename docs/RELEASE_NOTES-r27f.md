# AM Mod r27f

## Fixed: samples could not be assigned to pads

Loading samples from the built-in library or search could fail with
“Load Sample Failed” / “Could not load sample to pad”. Copy Pad and rendering
sounds or sequences to pads were also affected.

Device diagnostics confirmed that source files were readable but the shared
temporary `Temp-Project_[ProjectData]` folder was missing. The first-load SD
startup path could bypass its preparation. r27f prepares this workspace before
and after project loads, after New Project and recovery identity changes, and
when startup finishes without a Load. Existing audio and native project and
autosave paths are preserved. No SD reformat is needed for this bug.

**The author confirmed the correction on a real MPC Sample.** Thanks for the
sample names, reports and repeated testing that helped locate the cause.

The splash now shows **`version: r27f`** in the compact lower text band.
The boot helper, in-app splash and Mod Settings → About share the version.

## Also included since stable r26s

r27e was prepared locally, so this public prerelease also includes its changes:

- Mod Settings: five circular B2/B3 tabs, Standard/AM Mod lighting, configurable
  white working/source cursors, waveform/spectrum tint, palette role swaps,
  confirmed recommended reset and About.
- The browser cursor follows the actual selected load target, including empty
  Sample pads. Empty unselected destinations stay off; selected free/occupied
  destinations use green/red. Standard keeps 16 Levels Piano/Scale layouts.
- Hold Pad Bank and turn K to reset supported parameters; hold Pad Bank, then
  Shift and turn K for supported Shift parameters. Pads 1–8 still select banks.
- Project snapshots include Mod Settings and OptoCat/Warm; recovered projects
  regain their original display identity with first-Save confirmation.
- Single full project load at startup, Am MOD splash and normal-power recovery
  saving. Project → Mod Off is removed.
- Steps WRITE on SEQ RECORD and TOGGLE on SAMPLE RECORD are mutually exclusive.
- Browser ERASE, Note Repeat priority/Type redraw and spectrum tint corrections.
- The confirmed native K fix, OptoCat/Warm, filters/envelopes and EQ side meters
  remain included.

[Complete earlier changes](RELEASE_NOTES-r27e.md) ·
[Features](FEATURES.md) · [Known issues](KNOWN_ISSUES.md).

## Install and validation

Back up samples and projects before updating. Copy the ZIP's `autorun` folder
to the SD root, replacing the previous mod files, or open this ZIP in the
included installer. **exFAT is supported; FAT32 remains experimental.**
Installer 1.0.3 updates the shutdown hint after removal of Mod Off. Its Latest button selects stable releases, so
download `am-mod-r27f.zip` from this prerelease and select it locally.

The workspace regression has device confirmation. ARM64/QEMU and UBSan checks
cover workspace recreation, actual native file copying, preserved file contents,
Load result ABI, New Project/no-load startup, recovery, rendering adapters and
branding bounds. The final package's full hardware matrix and physical FAT32
coverage are not claimed complete. Other tester reports remain tracked.

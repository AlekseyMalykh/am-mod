# r26k candidate — wait for native dialogs before restoring a project

[Русский](RELEASE_NOTES-r26k.ru.md)

The mod could request its startup project while the stock application's
startup load still had a modal dialog open, including missing-sample/error
dialogs. Sample's per-component input-blocking check is insufficient: a native
modal can allow events on other components. Reading the XPJ successfully does
not establish that the full project has finished loading.

Startup restoration now waits until the native top-modal list is empty, then
starts a fresh 2.5-second quiet interval before requesting the full project.
The native dialogs remain available to the user; the mod does not dismiss them.
Diagnostics record a bounded wait marker and the exact selected XPJ path.
The newest-saved-XPJ selection policy is unchanged.

## Verification and limits

- ARM/QEMU and UBSan reproduce the old behaviour with the r26i workflow source
  and verify that the candidate waits, restarts the quiet interval after a
  dialog closes and makes exactly one full-project request.
- The full native VM reproduced a startup missing-sample dialog overlapping
  the old autoload. After aborting the unresolved load, an empty project was
  active despite the mod having marked autoload complete.
- A diagnostic build with the new guard waited through the missing-sample and
  subsequent error dialogs. After both were closed, it loaded the valid B
  fixture and its sample. Ordinary Save showed `Lifecycle-B`.
- Mod Off saved B with three note events at 0/960/1920, tempo 137, mixer volume
  0.25 and one available synthetic sample. The relocated copy had no missing
  sample or reference to its temporary staging directory in that test.
- Exit-copy worker failure checks, all 22 K/input regressions, OptoCat normal/
  UBSan checks and the full 50-callback preload constructor passed.

This fixes a reproduced startup ordering problem. It does not repair missing
source files or establish the cause of every `Could not load project/sample`
report from Leusha. Those reports still need their exact sequence and build.
The original A/B fixture files are kept separate from the recovery copy.

`Mod-Exit-Last` is still a separate exit/recovery copy. When that newest XPJ is
restored, its filename can become the default Save name. No change to that
naming policy is included in this candidate; the original-name UX remains
tracked separately as L04.

The device-confirmed native K delivery/takeover implementation is unchanged.
All r26j OptoCat changes remain: Shift Attack/Release/Knee, paw timing and the
maximum **+6 dB** gain. No compressor artwork or DSP was removed.

Firmware **1.3.0.12**; exFAT or experimental FAT32. Replace the complete
`autorun/` folder or install the r26k ZIP with installer 1.0.2. Existing projects
and samples can stay on the card. This is a local candidate, not a published
GitHub release; physical MPC verification is pending.

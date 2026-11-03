# r27e — release readiness

**State:** local prerelease prepared. Public stable remains r26s. This preparation
does not change remote branches, tags or GitHub Releases.

## Verified

- r27e build/shared version and all 50 preload callbacks.
- Current ARM/QEMU/UBSan reports cover128 pad indexes/eight banks/four cursor modes,
  native browser target setter, empty/no-program selection, source/target/Standard,
  Steps WRITE/TOGGLE, K/reset, Repeat, XPJ snapshots/recovery, missing Temp-Project,
  EQ/Filter and OptoCat/Warm.
- Package hashes, shell syntax; unchanged installer1.0.2 validation.
- r27e SD write/readback: 75 payloads, six XPJs and user files preserved. The refreshed
  release ZIP changes documentation only; autorun remains byte-identical.
- Earlier MPC confirmation exists for DRM splash visibility and native K end stops.

## Repeat on MPC before stable promotion

1. Edit a new project, wait for autosave and test manual Save/Load without Auto Save Error.
2. A/B/recovery copy notes, sequences, samples, effects, identity and first-Save prompt;
   normal power-off, stock without the mod card, then mod restart.
3. Empty Sample/browser cursor and bank changes; Copy/Batch/Chop/Mono/Render sources,
   free/occupied targets and all cursor speeds; Standard/Piano/Scale.
4. Fast-release/latched Repeat, B1–B3/Type, OptoCat/Warm/Bypass after New Project.
5. Original L06/L11/L13 cases and physical FAT32.

Earlier subsystem native VM tests are not an end-to-end boot of this final
package; neither that full VM pass nor full hardware coverage is claimed.
The appropriate channel is **prerelease**, without closing every tester report.
Synth/stems/mixer are future requests, not unfinished features of this release.

[Complete changes](RELEASE_NOTES-r27e.md) · [Report status](KNOWN_ISSUES.md).

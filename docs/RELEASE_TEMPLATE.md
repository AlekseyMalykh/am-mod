# Release description template

One layout for every GitHub release. The release body is English with a full Russian copy; links point to the release tag, not to `main`.

**Title:** `Am MOD rNN — <two or three main changes>`
**Tag:** `rNN`. Mark *Pre-release* for test builds; the stable build is *Latest*.
**Assets (always the same set):** `am-mod-rNN.zip`, `MPC-Sample-Mod-Installer.exe`, `SHA256SUMS.txt`, `RELEASE-VERIFICATION-rNN.json`, `xdg-dbus-proxy-source.tar.gz`.

```markdown
MPC Sample **1.3.0.12** · exFAT (FAT32 experimental) · Installer 1.0.3+

**Back up projects and samples before updating.**

## What's new
- **Feature.** Where it is and what it does, in one or two sentences.

## Fixed
- **Area.** What was wrong and what happens now.

## Tested
On a real MPC: … · In the emulator / ARM checks: … · Still to check: …

## Install
Copy the complete `autorun/` folder from the ZIP to the card root, or pick the ZIP (or *Latest release*) in the installer.

SHA-256 `am-mod-rNN.zip`: `<hash>`

[Known issues](https://github.com/AlekseyMalykh/am-mod/blob/rNN/docs/KNOWN_ISSUES.md) · [All changes](https://github.com/AlekseyMalykh/am-mod/blob/rNN/docs/RELEASE_NOTES-rNN.md)

---

## По-русски
(тот же текст по тем же разделам: Что нового · Исправлено · Проверено · Установка)
```

## Old releases

Add one line at the top of older release bodies when something there is outdated, for example:

- r26a, r26d: `> Outdated: FAT32 is supported experimentally since r26e. Use the latest release.`
- r27f, r27n: `> Superseded by r27p.`
- r26s: replace links to `blob/main/...` with `blob/r26s/...`.

## Repository “About”

- Description: `Unofficial SD-card mod for the Akai MPC Sample: step sequencer, chords, search, pad EQ, resampling, rear recording. Nothing is flashed.`
- Topics: `akai`, `mpc`, `mpc-sample`, `sampler`, `firmware-mod`, `music-production`, `sd-card`
- Website: link to the latest release.

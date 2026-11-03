# r27f release verification

Published as a prerelease; stable r26s is retained.

- Author confirmed the temporary-workspace correction on MPC Sample before
  requesting publication. The tested candidate was r27e-workspace-fix1.
- r27f retains that fix and refreshes the shared version, `version: ` splash
  prefix and release documentation.
- Actual native copy failure with ENOENT is reproduced before initialization
  and corrected after it; workspace recreation and existing-file preservation
  are checked under ARM64/QEMU and UBSan.
- All 51 compiled callback slots, full preload constructor, current branding,
  native recovery/settings and scoped render adapter checks are verified.
- OptoCat/Warm and native takeover code remain included.
- Public asset SHA256 checks and installer validation accompany the release.

The final r27f package is not claimed to have an exhaustive device or full VM
run. Physical FAT32 and remaining original tester scenarios stay open. See
[known issues](KNOWN_ISSUES.md) and [release notes](RELEASE_NOTES-r27f.md).

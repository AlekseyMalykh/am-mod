# r26e candidate — experimental FAT32 support

[Русский](RELEASE_NOTES-r26e.ru.md) · Changes since [r26d](RELEASE_NOTES-r26d.md).

- Start AM Mod from **FAT32 / MS-DOS (FAT32) experimentally**, as well as exFAT on firmware **1.3.0.12**. Physical MPC FAT32 testing is still pending. Existing FAT32 cards do not need reformatting or moving projects and samples off the card.
- The loader, sandbox, diagnostics and ARM save/journal flush helper all support the detected filesystem. Only the two native SD save folders remain writable in the application sandbox; filesystem, canonical path and device/inode checks remain enforced.
- Installer **1.0.2** accepts FAT32 on Windows/macOS/Linux and checks package capabilities. Older exFAT-only packages are rejected on FAT32 before backup or writes. Use the complete r26e `autorun/` folder; updating only one launcher file is insufficient.
- The r26d application library is unchanged, including pad colours, project restoration and Filter/Filter Env/EQ spectrum with native curves.

Keep a volume name such as `MPC` or `MPCSAMPLE`, using ASCII letters/digits and `.`, `_`, `-`, without spaces. This applies to the card name, not project/sample names. FAT32 limits each file to **4 GiB minus 1 byte**. NTFS, APFS and ext4 are not supported installation formats.

## Validation

- Installer regressions: installation, backup, reinstallation, removal, preserved user files, OS filesystem aliases and rejection of old packages on FAT32.
- Production shell guard tests: accept exFAT/FAT32, reject filesystem substitution, read-only save folders, missing folders and additional writable disks.
- Disposable real FAT32 and exFAT images: byte-verified installation/reinstallation/removal, preserved project with Unicode filename, ARM `syncfs` for saves and journal; reject wrong filesystem, changed identity and symlink metadata.
- Full native emulator FAT32 validation passed: stock autorun, sandbox, automatic project restoration, native Mod Off save and stock application recovery. The saved XPJ retained three MIDI events, 137 BPM and mixer/track volumes; the original project and a sentinel user file were unchanged. The saved file was verified again after stopping the VM and mounting its image read-only.
- Slow ARM software emulation needed a RAM-only increase of the recovery service timeout to 600 seconds and a retry of recovery after its original timeout. Release files and timeout settings were unchanged.
- exFAT passed the real-image installer/flush tests and the native emulator sandbox probe. Its first emulator attempt returned to stock after a startup timeout during a VM pause; the repeated probe passed. The complete application restore/save cycle was tested on FAT32, not repeated on exFAT in this check.
- Physical MPC FAT32 testing is pending. This experimental candidate has not been published.

Reproduce the storage checks from `sd-mod-session/` with `python3 verify_filesystems.py` and, on Linux with the ARM toolchain/sysroot and filesystem tools, `sudo unshare --mount --propagation private python3 verify_native_filesystems.py SYSROOT ../am-mod-r26e.zip`. Build the candidate with `python3 package_filesystems.py PREVIOUS_R26D.zip r26e`; it preserves the previously verified app payload.

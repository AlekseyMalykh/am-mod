# Am MOD r30b-usb11 — device-side USB audio names

Am MOD / Master + A–H now supplies a consecutive USB channel-name table: Master 1, Master 2; Track A 3, Track A 4; …; Track H 17, Track H 18. Stereo pairs are Master 1/2, A 3/4, …, H 17/18. No REAPER or Windows settings are modified.

A read-only device descriptor capture confirmed 18 channels with iChannelNames=0; ASIO labels matched unrelated USB strings. The published inMusic UAC2 implementation also leaves the name indices unset.

The GPL am_usb_names.ko module is built from inMusic's exact linux-imb-6.12.60 archive. It adds the channel strings and sets both Input Terminal and AudioStreaming name indices, using a non-spatial channel configuration for independent tracks. It reuses the built-in ALSA audio implementation.

The launcher verifies the module SHA-256 and stages it in RAM. With the app stopped, the host removes the gadget, temporarily registers the named UAC2 function, and recreates the multichannel gadget. Before SD Card Access, Stock/Stereo or recovery, all named instances are removed, the module is unloaded and the original driver registration restored. No eMMC firmware or kernel is rewritten. Only kernel release 6.12.60-imb-2025-12-03-rt13 is supported; preparation failures restore stock USB.

Validation: eight host lifecycle scenarios; the original MPC kernel from part2/kernel.fit booted in QEMU; actual reads of all 18 USB string descriptors; active module removal refused; two named-to-stock cycles. The test controller exercises EP0 descriptor enumeration only, not ISO audio streaming. Windows/ASIO labels and real audio recording still require MPC testing.

Restart MPC after installation. Enable Am MOD / Master + A–H, restart after a setting change, reconnect USB and reopen the DAW audio device. Stock and Stereo retain the stock stereo master. B3 SD access return was previously physically confirmed.

Corresponding GPL source, headers, configuration and reports are included under SOURCE/usb-audio-names. Official archive: inmusic-software/az0x-open-source branch mpc-sample/1.3.0, linux-imb-6.12.60+git-r0-patched.tar.xz; SHA-256 9d6999d9f69f5afd396a634facadc9219dcb85407264273d912355dd6db38f9b.

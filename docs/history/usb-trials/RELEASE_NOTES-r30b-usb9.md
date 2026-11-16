# Am MOD r30b-usb9 — B3 MIDI input setup

The usb8 log confirmed INPUT_MODE_FAILED errno=22. The helper called RAWMIDI_IOCTL_DROP for an input stream, but the kernel accepts DROP only for output. It failed before setting standard MIDI mode.

usb9 uses RAWMIDI_IOCTL_DRAIN with stream=INPUT, which clears the input queue without waiting for output. It then sets RAWMIDI_PARAMS mode=0 and handles the actual B3 Note On/Off messages. The regression model now rejects input DROP with EINVAL and accepts input DRAIN. DRAIN and PARAMS failures have distinct log entries.

Safely eject the disk on the PC → B3 Disconnect → B3 Return to Am MOD. B1 cancels. Keep the cable and card connected. Restart the MPC after installation.

Retains usb8 Stock stereo USB master and hidden FX View; Am MOD offers Stereo / Master + A–H, with master 1–2 and eight stereo tracks 3–18. B3 exit and multichannel recording still require hardware validation. Automated checks and AArch64 compilation do not establish physical success.

ABI reference: https://github.com/torvalds/linux/blob/v6.1/sound/core/rawmidi.c — RAWMIDI_IOCTL_DROP, RAWMIDI_IOCTL_DRAIN and snd_rawmidi_drain_input.

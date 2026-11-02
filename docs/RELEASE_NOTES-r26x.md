# AM Mod r26x — local candidate

[Русский](RELEASE_NOTES-r26x.ru.md) · [Readiness audit](RELEASE_READINESS-r26x.md)

**Unpublished.** Public download remains r26s. Combines local r26t–w changes
with the following corrections; this does not claim every report is resolved.

- **Note Repeat / Type:** hidden overlay paints cannot reclaim the header or
  mark the overlay active. Restore the compressor header once after close.
  Checks cover queued hidden paint and32 subsequent Type frames without a
  repaint loop; the original intermittent MPC flicker still needs confirmation.
- **16 Levels in Standard:** Lights, musical Piano/Scale lighting,
  tonic/scale/Transpose16/role colours remain available. Pad/Seq Colour stay disabled.
- Retains Default + five user colours, reserved system hues,400/400ms white cursor,
  double source pulses, selected green/red targets, unselected empty targets off
  and grey recorded-event flashes for muted sounds.
- Retains Standard/AM Mod, seven Project entries/native six-row viewport,
  Mod Off last, Repeat B1–B3 priority, OptoCat/Warm/GR/sleep/shared Bypass,
  Gain −6…+6 dB, EQ and the hardware-confirmed K fix.
- Updates README, FEATURES/KNOWN_ISSUES/FEEDBACK in both languages, Leusha's
  tracking list, colour map and CHANGELOG. Thanks to Leusha for detailed testing
  and ideas; no new external replies were sent.

ARM/QEMU normal/UBSan and source SHA-256 checks cover specified boundaries and
fixtures, not the complete device. No new full native-main run or physical r26x
trial was performed. [Required acceptance and open P1 reports](RELEASE_READINESS-r26x.md#required-before-a-stable-release).
FAT32 is experimental; exFAT supported. Copy the complete `autorun` folder to
the SD root; existing projects/samples can stay. Firmware1.3.0.12 only.

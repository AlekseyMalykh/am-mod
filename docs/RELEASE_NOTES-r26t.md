# r26t candidate — selected pads remain visible

[Русский](RELEASE_NOTES-r26t.ru.md)

Selected sources in Batch Copy and selected slices in Batch Extract now pulse
white for 240 ms and show their assigned/source hue for 400 ms. Unselected pads
retain their steady hue. This fixes the reported case where every Chop is purple
and selection membership disappears. A white assignment still pulses in brightness.

The same source cue applies in Stereo→Mono and Resample. Assigned targets retain
their white/hue cursor, offset from source pulses; Stock targets stay white and
red overwrite warnings keep their red pulse. The Batch hint now says White blink.
Ordinary native Copy Pad sources also pulse: the native selected-source bitset
emits full red, distinguished from browser/overwrite/Mute warnings using the
validated native mode. Sequence Copy no longer receives sound-pad recolouring.

In the Chop/Tune event picker, the chosen value alternates white with its base
colour. Chop values use their source sound's Pad Colour; original-value amber is
preserved between cursor flashes. Ordinary native Chop keeps its orange marker;
Sound selection, Steps grid/playhead and accepted Seq pulse timing remain intact.
No project metadata, event content, preview/audio action or Undo rules change.

ARM/QEMU and UBSan exercise the real native Copy lighting source mask (both64-pad
halves), RGB ownership/warnings, all13 colours/128 pads, purple Batch slice pages,
eight Copy banks, picker values and existing workflow/native command checks.
The full preload constructor passes; full combined native-main and physical MPC
selection checks remain pending. r26s features, EQ strips, Warm and Opto Gain
−6…+6 dB remain. Local candidate, not published.

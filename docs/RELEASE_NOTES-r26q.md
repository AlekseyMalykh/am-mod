# r26q candidate — Seq pulses in tempo

[Русский](RELEASE_NOTES-r26q.ru.md)

Seq selector pads retain their assigned hue and pulse briefly to full brightness:
selected once per bar while stopped, playing once per beat, queued twice per beat
until the actual handoff. A visible background remains between flashes. Other
occupied pads stay at the same level before and after Play. Empty slots remain
off unless selected or queued. Recording and native Mute ownership are preserved.

Tempo and meter come from the validated native playhead on the UI thread.
Playing pulses follow the native 960-tick quarter-note clock, interpolated between
updates; stopped pulses use elapsed monotonic time and the current tempo/meter.
The fallback is 120 BPM, 4/4 if the native playhead is unavailable. Flash width is
at most 90 ms and one third of the cycle; background 96/255, peak 255/255,
other occupied pads 192/255. Native breathing still identifies queued targets.

ARM/QEMU and UBSan check 20/60/120/137/300 BPM, 3/4 and 6/8, live tempo changes,
loop/seek/Stop, clock interpolation, assigned colours, empty queued targets,
stable other-pad levels and native Mute/recording/Steps boundaries. OptoCat,
Warm, common Bypass, New Project master rebinding and all r26p fixes remain.
Full native-main and physical MPC pulse/queue validation are pending. Local
candidate, unpublished.

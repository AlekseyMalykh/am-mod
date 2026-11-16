# Am MOD r30b-usb12 — tuner and live FX edits

Sample tuner pitch detection now runs on a low-priority worker using copied PCM. Native sample reads stay on the model owner; project/sample/trim/rate invalidate the cache and stale project results are rejected.

Parameter edits preserve unaffected FX slots. EQ and compatible custom effects keep their DSP state and smooth targets. A changed stock processor is prepared off audio and crossfaded for 20 ms in its own slot; stock setters lock internally and are never called from audio. The changed stock effect itself restarts its history. Shimmer Size/Predelay and chain topology changes still require prepared replacements.

FX Chain B2 now bypasses the selected effect; Shift+B2 remains Paste. The area below the Track shows slot/status. Input FX/ROUTE tabs retain their actions.

Targeted continuity/tuner regressions pass on host, UBSan and ARM/QEMU. Mixer control/view and pinned native FX DSP checks pass. Legacy broad resample UI and Rear Input fixtures have outdated expectations and are not claimed to pass. Audible results require hardware testing. Packaging does not write a card.

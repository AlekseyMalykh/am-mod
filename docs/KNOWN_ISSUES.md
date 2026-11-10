# Known issues, testing status and requests

Updated **6 November 2026**. Latest release **r27r**; r27q, r27p and r26s remain available as older versions.
[Full changes](RELEASE_NOTES-r27r.md) ·
[Settings](MOD_SETTINGS.md) · [Original Leusha list, Russian](TESTER_FEEDBACK_LEUSHA.ru.md).

Implemented means the change and stated coverage exist, not that the entire
original hardware report is closed. Mod Off is removed: current tests use normal
power-off, External saves and stock startup without the mod card.

| ID | Status | Implementation / next check |
| --- | --- | --- |
| BUG-07 | Fixed in r27r; confirmed on MPC | Recall (Shift + Sample Record) froze the MPC (Leusha, three times). Only Stop now waits for the audio render; Recall works with Resample and Rear sources. The exact native cause of the old freeze was not proven; the protective change removed it on the device. |
| BUG-02 | Fixed in r27r; confirmed on MPC | Seq Colour (Shift+K1) kept the old controls. |
| BUG-03 | Fixed in r27r; confirmed on MPC | Mix encoder cycle after K2 Pad Colour; with Shift, Volume and Pan stay and only K2 is Kit Volume. |
| BUG-04 / BUG-05 | Fixed in r27r; confirmed on MPC | Chop pads dark or wrong colour on entry, after Extract and after leaving Chop. No native repaint is requested inside Chop. |
| BUG-06 | Fixed in r27r; confirmed on MPC | Incomplete screen and blinking “Restored” note after autoload until a pad was pressed. |
| BUG-01 | Fixed in r27r | Scaled takeover jump after EQ → Filter. |
| P01 | Changed in r27r; confirmed on MPC | One set of pad lighting rules: white cursor and selected sequence; Copy source blinks, free/occupied targets green/red; Mute keeps colours, muted red; Erase keeps colours; brighter pressed pads; no green flash on a short Pad Bank press; source lit at once in Resample → B3. |
| P02 | Changed in r27r; device check pending | Copy Sequence colours and transfer of the sequence colour to the target. Sample / Seq leaving Mod Settings, Key / Scale, Search and Stereo > Mono. |
| S01 | Changed in r27r; confirmed on MPC | Search: Internal and All (Internal + SD), complete SD results, steady K3 Volume, stock Internal / External tab, no Knob FX over the browser, B2 idle while typing. |
| O01 | r28c trial | Sample returns from 16 Levels and Chop in r28c. This route passed an ARM input check. Seq exit and the Sample gesture on hardware still need verification. |
| O02 | Request | Spectrum for a muted pad (it does not play; would need a whole-sample spectrum). |
| Q01 | Added in r27q; device check pending | Degrees layout: pads play key notes, I/IV/V colours, Transpose16 by degrees, MPCS19 colours. Check every scale, an original note outside the key, Steps playback flashes and Save/Load. |
| Q02 | Changed in r27q; device check pending | Search: Folder/Library/SD Card/All, live matches, preview on, encoder press enters a letter, Erase in Encoder input (also in the Save name window). |
| Q03 | Changed in r27q; device check pending | Rear Routing, Mod Settings and confirmations redrawn in the stock style; anti-aliased splash text. |
| N19 | Fixed in r27p; author accepted build | Knob FX owns K1–K3 over custom pages; FIFO boundary handles rapid FX entry. ARM64 routing checks pass. |
| N20 | Fixed in r27p; VM and ARM64 selection coverage | Compressor Tune Scale follows the actual selected sample, not hidden Sample labels. Orig Note selects Scale; all 128 indexes covered. |
| N21 | Fixed in r27p | First encoder click after leaving Mod Settings is forwarded; hidden Sample controls yield to Project. Ten rapid transitions passed in the preceding VM candidate; ARM64 regression test retained. |
| N22 | Fixed in r27p | EQ page retained after Seq → Sample. |
| R01 | Added in r27p; author accepted build | One-page Rear routing, direct-pad mono/dual recording with cancellation/overwrite confirmation; independent monitor/pan/level also in Input Config. Free recording only; see release notes. |
| N14 | Fixed in r27f; author confirmed on MPC | A missing temporary ProjectData folder blocked library/search imports, Copy Pad and render assignment. The shared workspace is prepared across project boundaries. The author confirmed the correction in r27e-workspace-fix1; r27f retains it with updated version branding. ARM64/UBSan reproduce the failure and recovery. [Evidence (RU)](SAMPLE_WORKSPACE_FIX.ru.md). |
| N15 | Fixed in r27n; exact device retry pending | Long-sample Copy Pad uses a dedicated SD workspace. Verified a 47.7 MB copy in VM and SD bind on exFAT on MPC. |
| N16 | Fixed in r27n; device confirmation pending | Active loops flash after selection moves, and SHIFT alone keeps other pads bright. Verified in VM. |
| L01–L04 | P1 / implemented; original hardware cases pending | Full project restore, original recovery identity/sample paths and separate first-Save confirmation are implemented. The author passed basic A/B cases, not every original Leusha case. Test normal shutdown → stock without SD using A → mod with B; compare notes, sequences, samples and effects. Historical Mod Off steps are obsolete in r27e. |
| N09 | Implemented; device check pending | Delayed New Project Auto Save Error: initialize a missing Temp-Project through native serialization. Relevant ARM/UBSan branches and an earlier native VM case passed; the exact final package was not booted end-to-end in VM. Create/edit a new project, wait for autosave, then test manual Save/Load. |
| L05 | Implemented; original device case pending | Physical ERASE deletes from Project Load with native confirmation. Retest files/folders and ordinary/search views. |
| L06 | Author: looks fixed in r27r | First-beat Chop light appears only after looping. Confirm on more projects. |
| L07 / L10 / L12 | Earlier confirmation | Sample playback Pad Colour, distinct Green/Cyan and native Scaled/K end stops fixed. The author confirmed K on MPC; issue #2 closed. This does not confirm every new colour workflow. |
| L08 / L09 / L22 | Implemented; full hardware cases pending | Assigned colours across Copy/Extract/Mono/Render/browser and Duck/OptoCat, plus Copy/Extract inheritance and Undo/Redo. Retest banks, Paste/Cancel/Undo, occupied targets, both compressors, Bypass and Save/Load. |
| L11 | Fixed in r27n; device confirmation pending | A muted pad shows grey waveform and spectrum in Trim, Filter, Filter Env and EQ regardless of tint. Curves and envelopes remain visible. Verified with VM screenshots. |
| L13 | Fixed in r27p; confirmed on MPC in r27r | Native Copy retains Erase after Shift release; Shift+Pad Bank reverses banks. Native VM A → H verified. |
| L16 | Implemented; device readability pending | Seq selection pulses per bar at Stop, playing per beat, queue twice per beat; background/other brightness stable. Test queue/cancel, tempos and meters. Standard keeps native lighting. |
| L20 / L21a | Implemented | Filter/Filter Env/EQ spectrum with curves and side strips. Pad Colour is Sample → B1 Mix → K2 in Am MOD; Shift+K2 Kit Volume, K1 Volume. |
| N01–N03 | Implemented; targeted device checks remain | EQ Volume/Pan; OptoCat−6…+6dB, Shift Attack/Release/Knee, Warm, paw/red GR and input bow; shared Bypass, New Project rebinding and active-type entry. Retest audio/meters, New Project and restart. |
| N04 / N05 / N10 | Implemented; device readability pending | White source/cursor uses Steady/Slow/Normal/Fast instead of the previous independent double flash. Unselected empty targets off; selected empty/occupied green/red. Eight user slots stay separate from system roles. |
| N06 / N08 / L19 | Implemented | Six Project rows without Mod Off. Five Mod Settings tabs, confirmed reset, Standard/Am MOD, tint/palette/pulse/About. Standard retains Piano/Scale/16 Levels. Display and OptoCat settings are stored in XPJ. |
| N07 | Implemented; fast/latched Repeat check pending | Repeat B1–B3 priority and hidden-paint guard prevent compressor Type indication returning on release. Original hardware scenario remains pending. |
| N11 | Implemented; device check pending | Pad Bank + K / Pad Bank → Shift + K resets supported factory values. Native Duck/other FX, Main Volume and object selection are not reset. |
| N12 | Implemented in r27d; device confirmation pending | Library cursor follows native load target, not stale editor instrument; search inherits it. ARM/UBSan covers128 indexes; native VM confirms the field. |
| N13 | Implemented in r27e; device confirmation pending | Empty selected Sample pad gets a white cursor and turns off between flashes. Checked128 pads/eight banks/four modes, missing program, stale loaded-pad cache and Standard. |
| FAT32 / #1 | Experimental | Supported in packages from r26e. Successful startup workaround used exFAT; closing #1 does not verify physical FAT32. Separate FAT32 install/restart/save testing remains. |

## Requests not included in this release

- Synth: **deferred by the author**, not being implemented now.
- Stem export: not implemented; mixer/effect routing is not designed yet.
- L15: an additional Key/Scale entry in Input Config.
- L17/L18: investigate low-frequency detail and spectrum brightness while keeping curves readable.
- L21b: an independent RGB editor; palette swapping is not a full RGB editor.
- L21c/L21d: interchangeable characters/artwork help; Duck/OptoCat B2 chooses an algorithm, not an independent skin.
- L14: Piano/Scale instructions exist; the [device walkthrough](FEATURES.md#quick-start-16-levels-pianoscale) can still be refined.

Ideas C01/C02: live held-chord/arp changes and force-based expression are [future requests](IDEAS.md), not implemented in r27e. New-hit velocity already passes through; continuous pressure still needs investigation.
Plan M01: audit MIDI In/Out, channels/clock, external Chords/arp operation, velocity and Note On/Off. Details are in the same roadmap; verification is pending.

## Limits

Autoload picks the newest saved SD XPJ, not last opened. Internal is temporary
inside the mod. Stock resaving may discard optional mod XPJ fields. K reset does
not cover native Duck/other FX. Newest cursors/final-package hardware confirmation
remain pending; a full VM boot of the exact final r27e is not claimed.

Special thanks to **Leusha** for careful, detailed testing: BUG-01…BUG-07 and most r27r changes come from those reports. Thanks also to mxt1iga and everyone providing tests and reproductions.

# Pad Steps: pinned native contracts (1.3.0.12)

Stock SHA256: `b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508`.

| Native object/path | Address / layout |
|---|---|
| StepEditScreen vptr / destructor | `0x6e24f38` / `0x16aea50` |
| Step controller from screen | `screen+0x108`; vptr `0x6d8ec20` |
| StepInfo | `controller+0x68`: index, time(+8), grid ticks(+0x10), window low/high, extent, count |
| Selected event | native Value `+0x108`, current index `+0x124`; normalize functor `+0x128` |
| Event window | `+0x280` begin, `+0x288` end; 56-byte values |
| Current event list | `controller+0xc0`; tree `list+0x60`, sentinel `list+0x68` |
| Native lower-bound / next | `0x27d00f0` / `0xb51738` |
| Native event container insertion / cleanup | `0x1574460` / `0x157df10`; 48-byte container, header +8, value at node+0x20 |
| Tag-3 note factory | `0x27e54d0`; x8 result, integer pad/time/duration/value/flags and s0 velocity |
| Tag-3 payload | time +0, variant/flags +8, velocity +0x10, duration +0x28, note +0x30; complete inline payload copied |
| Complete musical equality | `0x19722e0`; avoids comparing padding bytes |
| Pad↔note mapping | `0x157f800` / `0x157f7d0` |
| Add command factory | `0x19500e0`: empty removals, one added event, empty change map; native strings, x8 shared_ptr, ninth stack argument null |
| Delete / change command factory | `0x194fa30` / `0x194fbb0`; native command service `controller+0x240` |
| shared_ptr release | `0x123bca0`; result ABI explicitly adapted in `step_abi.S` |
| Native cursor time setter | `0x15814a0`; calls Value normalization/notification |
| Time signatures | `controller+0x250` UI proxy, `+0x578` const list; tick→BBT `0x29bf230`, BBT→tick `0x29bf150` |
| NowPosition Timer / vptr / engine | `0x2109a90` / `0x6da4588` / `+0x258` |
| Historical r13c query — actually record-mode None | `0x2133870` on NowPosition+0x258 engine → +0x768 transport facade; CurrentThings+0x88 was the wrong source |
| Physical −/+ | note codes `0x37` / `0x36` → GlobalControls `+0x2930` / `+0x29e0`; bindings `0x14af0a0..14af0dc` to OnMinus/OnPlus, lambda RTTI `0x6e09310` / `0x6e09320` |
| Physical Record/Stop/Play | `0x50` / `0x51` / `0x52` → `+0x2b40` / `+0x2bf0` / `+0x2ca0`; native transport bindings `0x14d4114..14d419c` |
| Physical fader | CC `0x14` → GlobalControls `+0x2df8`; StepEditFaderResponder constructor arguments `0x14ae8bc..14ae8ec` |

Event storage is a multiset: native `0x1574460` inserts another node for an equal time/note key. There is no occupied-beat veto. The pad toggle view treats multiple notes as a group to inspect in Events. Native Copy/Paste uses the additive factory directly; stock `0x1581960` searches for an existing note first and is deliberately not used to paste into a populated position.

All model/GUI access is confined to the existing UI Timer. Hardware producers publish numeric tickets containing epoch/page/sound/grid/Shift. A press retains its target across Follow/page changes, and stale epochs are dropped. Naming input has priority. Existing `consumed`/`passed` release routing is retained for pads that began before an ownership change.

The shared bridge stride is 96 bytes, preserving the existing 4 KiB RX bridge cave despite two additional native hooks. Entry PACIASP/BTI, displaced instruction guards, callback-table identity and RX/RELRO restoration remain checked by the builder.

## Offline verification limits

`verify_steps.py` runs the real ARM factory, event containers, musical comparison, time-signature conversion and result adapters under QEMU. The fixture replaces GUI and command execution boundaries, so it does not prove actual audio, Undo replay, project persistence, physical pad releases or device graphics. These remain hardware checks. The shipped library is also loaded using the firmware rootfs and the actual ARM RAM patcher is exercised on a temporary host-side copy.


## r13b contracts

- Time source: native getter `0x2113b10` with NowPosition+0x258, exactly as the original Timer calls at0x2109bc8; no pointer retained. The prior +0x238 UI property is updated by a queued task and is no longer the time source. Getter and wrap are exercised with actual ARM instructions in the fixture.
- Sample label: native pad→note0x157f800, Program instrument getter0x286bbe0, layer0 shared sample getter0x2803b90 (x8 adapter), Sample+0xa8 JUCE String. Shared sample remains owned until the name is copied; release0x123bca0. Empty/missing/expired sample is Empty. Native Program/layer getter instructions and UTF8 string ownership are checked with model fixtures.
- Erase note0x09→GlobalControls+0x23b0; native responder ctor0x14ae540..634 supplies that signal. Erase is consumed only in Steps; ticket records its state at pad press. Existing note no longer toggles off without Erase.
- Pad Bank note0x23→+0x2300 is passed to the original; ctor0x14ae1a8..218 passes that signal to PadBankControls. Generic leave-on-any-button removed. Logical Steps selection survives temporary invisibility/destruction until B2, project reset or queue fault; ownership is unpublished while hidden/blocked.
- B1 Sound hold has its own numeric ticket bit; sound numbering remains physical. Step time maps top-down with pad_step(pad)=(3-pad/4)*4+pad%4; notes themselves remain chronological.
- Shift+pad uses the same UI FIFO as Shift buttons and restores native routing for global shortcuts. Native MidiMessage short-copy contract0xddec50 and note classifiers0xddfed0/0xddff20: size at+16, inline<=8. Only inline messages are copied; larger payloads rejected. Release/pressure retains owner after mode changes.
- LCD no longer draws the4x4 pad mirror or header tab segments. Painting starts at y5, leaving the native transport strip y0..4. The fixture records production paint calls and asserts no pad cells, a continuous K3 bar, and no drawing into that strip. The preview uses a substitute font and a reference native strip, not a device screenshot.
- Grid includes five straight divisons plus triplets. Ticket grid field expanded to4bits. K2 selects base division, Shift+K2 toggles Triplet. The Events native Q is unchanged.
- Lights renderer, control mapping and focus skipping all use normal Mode/Tonic/Scale and Shift Transpose/Role/Colour. Orig Note warning ink is0xffff4040.

All new gestures, native modal handoff/return, actual Pad Bank interaction with the selected sound, Note Repeat popup behavior, playhead and font rendering need hardware verification. Native command replay/Undo and audio are not executed by the fixture.


## r13c: observed Stop while position advances, native bank, sample index

Returned r13b report has CLOCK values42,98,160,216,279,341,404 while the user sees Stop. It confirms the clock hook runs; the running predicate was wrong. Native engine method0x2113ab4..abc queries facade at engine+0x768 using0x2133870. r13c captures that predicate with ticks during the owning NowPosition Timer callback. It no longer applies the getter to CurrentThings+0x88. QEMU now executes the actual stopped-query instructions, with a separate wrong facade stopped while the real one runs, plus actual stop/restart checks. The previous fixture stub hid this defect.

Pad Bank: native PadBankControls constructor0x14d7d50 takes eight integer/pointer arguments (x0..x7; no incoming floating/stack args), selected-list model at owner+0x140; exact vptr0x6d8c6f0. It is captured after the native ctor. Destructor0x14d7460 clears identity/model before destruction. Model row count+0x50, selected row+0x158 are bounded1..8. Shift+K1 uses native selected-row setter0x1a36330, which invokes IntProperty setter0x13b3740 at model+0x108; that same row is used by native PadBankControls. All model use is on UI. Test initializes native Notifier0x236f0d0 and executes actual selected-row/property setter instructions, then native cleanup0x236f110. Device LED notifications/constructor handoff still need physical verification.

Sample name: Program getter0x286bbe0 takes the instrument/pad index; SequenceEvent uses the MIDI note map. Returned XPJ has assigned instrument2 and noteForPad2=38. Passing38 selected the wrong instrument. r13c passes Sound directly and keeps MIDI mapping only for event data. Regression uses instrument2 with MIDI38, with instrument38 empty, then real layer/shared-sample getter0x2803b90 and String+0xa8.

Two new lifecycle bridges remain in the existing4KiB bridge cave and use the builder's displaced-instruction/PAC/branch/table checks. Library loading, full native ARM patcher hash and previous archives are verified. No flashing or persistent write scope change.


## r13d corrections and new contracts

The r13c interpretation of 2133870 above was wrong: it tests record mode == None. Ordinary Play can satisfy it. Independent playing is the virtual method +18 on engine+768 facade+148; exact common implementation 1445640 reads byte+1f0. Combined 2133800 checks playing AND record. Fixture independently controls both and executes all real getters.

SEQ REC 50 is a local default-Off Write gate in Steps, handled in UI FIFO order. It never arms native recording. Common MIDI LED sender23b0de0 emits B0/CC/value; native state2 maps to value2 via23b0180/23b01f0. r13d/e incorrectly used Save24 for Chop (corrected in r13f). Record/Levels28/Chop4b overrides cache and restore native raw values, including updates during ownership. SEQ is not overridden. AC50 native input159f2f0 table49e3744 maps28→GlobalControls+2720,24→+27d0; LED properties+2778/+2828. Native lighting14bd2c0 uses16Levels type8 for Slice.

Erase group uses19500e0 with owned removal multiset, empty additions/change map and one title/merge pair. Preflight and collection use the same bounded page scan (4096 events), same selected-sound and rounded-slot test, and final-page tail clamp. No borrowed source node survives mutation. Identical duplicates remain distinct. Both trees and constructed native Strings are released on success/failure; null/unconstructed Strings are not passed to native destructor. Single off-grid note deletes by complete original value via194fa30. No sample deletion.

Events Erase tap deletes a complete selected-event snapshot only on release, provided no pad/control/other action changed context or selection. Native Erase down and release are forwarded exactly once, including modal handoff; Erase+pad travels through UI FIFO before its release and suppresses tap deletion. The pad retains native release ownership. Confirmation is native QuestionScreen, excluded from header overlay to avoid Answer identities.

Event picker stages a complete tag3 value. Entry from28/4b (24 was an r13d/e error); B3 applies, B1/B2 cancel, B3 in main Events has no delete action. Apply re-resolves complete source equality, not row index. Shift+K3 applies a native candidate directly. Native16Levels processor21472f0 is called through the unhooked original trampoline with stack-only properties and a copied event. Modes0..9 are Velocity/Tune/Filter/Layer/Attack/Decay/Probability/Ratchet/Slice/Articulation. Packed parameters0/2/14/7/8/15 live at event+14; getters4230/setters4400 under27e0000. Probability+32, ratchet+34 low3 bits, articulation+33 (197 default) are distinct. No persistent original-input-mode marker exists. Pitch and Slice page overrides use native setter. Other fields, source sample, velocity/duration except their explicit mode remain preserved.

Ordinary playback context: SampleScreen+148 resolves CurrentThings (constructor16737b8..7e8, RTTI GOT7165810); native Step controller binding1583280..32d8 resolves CurrentThings+3d0 clip→+688 event list. Program+b8, active Levels instrument+668, sequence+1c8 (r13d/e incorrectly used ordinary instrument+60). Context is consumed on UI and not retained; pointer identities only reset numeric scan state. UI scans events crossed by the independent native transport clock, max4096 nodes, no audio callback or synthesized clock. Skip large seek/stall spans. RGB pulses120ms, then restore per-pad native cached colour. Naming keyboard retains priority. Global16Levels state is published numerically from existing lighting refresh, with no retained property pointer. Audio/graphics/actual device notifications remain unverified by fixture.

Playhead and playback pulses are cyan00c8ff. All ordinary populated steps green; Erase red; selection amber. Mute was absent in r13d; r13e adds native flag-bit9 group replacement, violet muted steps and physical Mute ownership.

## r13e native contracts

- Sound palette uses actual Program instrument index, then HasSample2804760, checking all layer shared sample references; no borrowed pointers cross the UI call. Native factory/lookup under QEMU distinguishes populated, empty and selected pads with a nonidentity MIDI map.
- Mute0x2b dispatch159f40c → GlobalControls25c0; AC50 binding14af104..11c →14ab200 →mode30f4; mode registration14aef20..28 maps it to MutePadMode1530fb0. Native event IsMuted2ac09bc shifts tag-word bit9, Mute Selected Events29e2020 ORs200. Group mute snapshots original events and replacement copies into owned multisets before one19500e0 dispatch; every other byte preserved. This verifies command construction and scope, not live native Undo replay or audio callback behaviour.
- EVENT_LABEL is a callsite-only String-result bridge at16a8418, where x24 is the current event and x8 the native nontrivial String destination. It appends fixed C/L positions to the Pad label; nonmusical events retain native2d444e0. Default callback fallback also uses native formatter. Source identity of a plain Velocity event cannot be reconstructed from this format.
- Chop availability additionally uses native shared sample getter2803b90, sample slice-data getter2941ff0, then vector60/68. Shared references are released before returning. Actual slice count caps the selector where available. The real user project is still required to verify recorded Chop variants beyond the packed Slice15 representation.
- Text picker K1Type/K2Choice/K3Range replaces the LCD pad squares; mode changes stay staged until Apply. Labels and native model/event data remain separate.

The final picker is not Tune-only: its mode is inferred from active packed modifiers/nondefault Probability, Ratchet or Articulation, otherwise Velocity/current matching native mode. It executes all ten pinned native16Levels modes on staged copies, not an ad hoc pitch mapping. Entry is the matching physical Levels/Chop button, main Events B3 stays free, B3 in picker applies. SEQ keeps its untouched native indication. Cyan playback and per-pad restore cover ordinary active16Levels plus picker using CurrentThings without needing a live Step Edit component.


## r13f — device evidence and corrections

Returned eventcheck.xpj has25 notes: Slice15 on MIDI36, pitch0 on37, filter2 on41, ordinary40. Full card including project and journal backed up before edits. C/L, pitch editing and mode inference confirmed by user; ordinary playback/Chop failed in r13e.

Physical Chop is CC4b: real native159f2f0 dispatches4b→GlobalControls2510,24→27d0. Binding1541ac8..1ae8 links2510 to1541bc0, testing type8 at1541c20. CC24 reaches SampleSaveMode1544680 registered30f8 via14ab580. Chop LED is2568. Prior tests injected a mistaken constant rather than testing its physical dispatch.

ChopPadsLighting152a870 reads Levels properties+f8 and bypasses SixteenLevelLighting1545930 (15459c0 returns early for type8). Added a pinned CHOP_REFRESH bridge publishing numeric state only. Native152a8f8..a908 uses CurrentThings668 as locked instrument; ctor152aed4..aefc resolves CurrentThings RTTI GOT7165810. Slice keys are1..16 independently of Sound bank.

Known UI Timer initializes playback UI identity even before Step Edit paint. Hidden Steps no longer blocks Sample. Whole Sample visibility gates outer-bank mode10. Mode10 maps events through program14a8 to visible source pads. Modes0..9 use the locked instrument and copied native candidate values. Picker uses its source and staged range. Stop clears pulses; context changes reset history. Bounded crossed-tick traversal wraps at sequence start, skips long seek/stall spans, excludes muted events. This displays scheduled events, not audio voices; random probability and track mute decisions are not observed.

At most32 numeric PLAYBACK gate/state transitions/pulses are logged. Gate bits(hex):1 hidden display,2 stopped,4 no clock,8 no program,10 no list,20 invalid source/bank,40 invalid mode,80 invalid time/length,100 visible Steps. No sample/project names.

ARM tests execute physical dispatch, cold UI, hidden Steps, locked instrument668, outer source pulses, all10 native modes through wrap and muted exclusion. C/L/CL compact. Grouped Steps rendered from production draw trace with substitute font; actual device LED timing/lifecycle handoff remains to verify.


### r13f1 callback handoff correction

Final review found the first r13f positional callback initializer placed Chop after NOTE_FOOTER/MUSIC_PROJECT while generated HOOKS placed it before them. The first card copy was immediately withdrawn; user instructed not to launch it. No device execution is confirmed. The rejected frozen r13f payload is retained as evidence.

r13f1 uses designated H_* callback slots and refuses null entries. verify_callbacks.py inspects the shipped ARM library's static compiled vector and actual R_AARCH64_RELATIVE/ABS64 function relocations, checking all38 names against generated hook order. It rejects a swapped Chop/footer vector. This verifies the handoff that the previous isolated step fixture did not cover. Other r13f functional changes above apply to r13f1; hardware validation remains pending.


## r13g — contextual event editing in both views

Returned r13f1 journal confirms BRIDGE_READY, outer/inner playback pulses and a picker with mask0000: the latter was permitted by source slice availability. User reports unmarked rows opening editors and no opening from Steps.

Use event_categories for labels and availability: bit0=C Slice, bit1=L persisted Levels evidence. Remove ordinary-note and sample-has-slices availability fallback. Own both unshifted buttons while Step Edit shows, even when unavailable, so they cannot fall through to native performance modes. Shift routing unchanged. Shift+K3 also requires C/L.

scan_page_into aggregates category flags per slot. Steps lights use cursor/sound's slot, not an unrelated native selected row. On open, bounded lower-bound traversal copies matching events for this sound/rounded step, including the final off-grid tail. One opens directly; multiple show menu4 list. Capacity64 fails closed; scan budget4096. No borrowed source nodes retained. Project/clip/queue invalidation closes the chooser.

menu4 consumes pads and Record without adding events or arming native recording. Encoder/K2/+/- choose; B3/press opens, B1/B2 cancel. List displays position, type/value, velocity and M for muted. picker_begin and Apply re-resolve complete equality, never an old list index. Single-event Undo command changes only that event; Steps/Sound/cursor retained.

ARM regression: unmarked note with a sliced source has no editor; single Steps Apply/Cancel; mixed group filtering; multiple-choice Apply preserves sibling and muted/velocity/time; stale deleted source rejected;65 matches refuse without mutation; final tail included. Production chooser paint traced with bounds checks. Device timing and Undo replay remain hardware validation.

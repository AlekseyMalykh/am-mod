# Changelog

**English** · Russian: [docs/FEATURES.ru.md](docs/FEATURES.ru.md), [sd-mod-session/IMPLEMENTATION_HISTORY_RU.md](sd-mod-session/IMPLEMENTATION_HISTORY_RU.md)

All builds target MPC Sample firmware 1.3.0.12 and run from the SD card; none of them writes the firmware. Details of each build are in `sd-mod-session/R*_RU.md` (Russian). "Confirmed" means checked on a real MPC Sample.

| Build | Date (2026) | Changes |
|---|---|---|
| **r24x** | 31 Oct | Search results light the pads with the shared palette: target white, red and blinking when it would replace a sound, occupied pads dim white. |
| **r24w** | 31 Oct | The mod is now called **AM Mod**: packages `am-mod-*.zip`, `AM-Mod-Installer.exe`, start screen "AM Mod". No functional changes from r24v. |
| **r24t–r24v** | 31 Oct | Resample target chosen on a 4×4 bank grid (source outlined, overwrites red). **Live spectrum** of the edited pad behind the EQ curve (dim amber from r24v). |
| **r24q–r24s** | 31 Oct | **Pan** and Tune bars grow from a centre mark. **Block layout** for the Chords, Steps, Stereo → Mono and Resample windows and the Steps edit menu. Knob FX works inside Chords. |
| **r24k–r24p** | 31 Oct | Seq pads: **Stock** colour by default, the playing sequence blinks once per beat and the queued one twice, an empty queued pad stays visible; with recording armed the Seq screen stays and the pads show the Sample layout. **Note Repeat** in Chords repeats the held chord at the stock repeat rate. Queued buttons are passed to the stock app in step with the audio (crash on Stop fixed). |
| **r24e–r24i** | 30–31 Oct | Chords **arpeggiator** on K3 (Off by default; Shift+K3 rate 1/4…1/32, Shift+K1 range); plays only while roots are held. **Stop pressed twice** no longer crashes: releasing all voices waits for the audio render. Tap Tempo works in Chords; Shift no longer looks held on entry. Chord playback lights were tried and removed. |
| **r24b–r24d** | 30 Oct | Chords: physical pad releases, stock Poly switched on at entry. One meaning for pad colours across Steps, the event editor, Batch, Mono and Resample. Mod-Exit copies named by date and time. Crash backtraces in the card log. |
| **r24a** | 30 Oct | Chords entry fixed: background Sample observers no longer block it. |
| **r24** | 30 Oct | **Chords** mode in 16 Levels Tune → Chop: scale degrees, 6/7/maj7/9, Maj/Min/Dim/Aug/Sus4, octave, inversion, Latch; recorded as separate Tune events. |
| **r23a** | 29 Oct | Resample includes Pad Link and Bypass (confirmed). Confirmed: Poly recording, boot with the card inserted, Orig Note save/reopen. |
| **r23** | 29 Oct | **Power key**: stock question, project copy to SD, power off (confirmed). Sequence lighting confirmed. |
| **r22–r22c** | 28–29 Oct | Project copy to SD on Mod Off; autorun without an extra question; **Poly** chord recording as separate events; 12 sequence colours (Shift+K1 in Seq). Test boot animation removed in r22a. |
| **r21–r21d** | 28 Oct | Several notes in the Tune event editor; note entry on empty Steps; **Stereo → Mono** (Shift+B3); amber Note Repeat and Knob FX panels; lighting fixes. |
| **r20–r20g** | 27–28 Oct | Fast resample of a single pad with its processing (confirmed in r20g). |
| **r19–r19d** | 27 Oct | **Pad EQ** page Sample → Filter → Filter Env → EQ (audible, confirmed in r19c); stock Seq/Sample resample entry. |
| **r18–r18a** | 26–27 Oct | Experimental FX chain for resampling; later withdrawn in favour of the stock FX Select and the pad EQ. |
| **r17–r17d** | 26 Oct | Resample entry (Shift+Pad 11); text cursor, Backspace/Delete in pad typing; Steps "Other Sounds"; Lights in the Tune event editor; playback blinking in Sample/16 Levels/Chop. |
| **r16–r16d** | 25–26 Oct | Readable Steps, Copy/Extract and search screens; audition in the Chop / 16 Levels event editor (L confirmed). |
| **r15–r15h** | 25 Oct | **Batch Extract** (confirmed in r15c); C/L event editor; preview volume (K3); copy with events; Lights Degrees; search auto-preview; Erase = stock Delete in User; Mod Off moved to the end of Project with a confirmation. |
| **r14–r14f** | 24–25 Oct | **Batch Copy** of up to 16 sounds (confirmed in r14e); **search** in Sample Select / Open Project; Steps mute. |
| **r13–r13g** | 24 Oct | **Steps** on the physical pads; moving events along the grid in Step Edit; Copy/Paste of events. |
| **r12–r12b** | 24 Oct | Project **Key / Scale**, **Lights** page, mod settings saved in the project; unified top buttons and tabs. |
| **r11** | 24 Oct | **Orig Note** of a sample. |
| **r10–r10b** | 23–24 Oct | **Typing names with the pads** (phone keypad). |
| **r9–r9h** | 23–24 Oct | Saving to **External** (SD) from inside the mod; Mod Off. |
| **r8–r8a** | 23 Oct | Redesigned **K1–K3 panel**: names and values always visible, amber focus, S marks. |
| **r3** | 23 Oct | First display patch: K1–K3 values (`sd-k-value-peek`). |
| **clone trial** | 21–23 Oct | Loader: verified copy of the stock app runs from RAM, stock app restored on exit or failure (`sd-clone-trial`). |

# AM Mod r27p

MPC Sample **1.3.0.12**. Changes since public r27n; includes its features and fixes.

- **Rear recording and monitoring.** Sample Record → B2 Routing: K1 chooses Record Rear L / Record Rear R / Record L + R. Press a pad to record mono. For L + R, choose the L pad, then a different R pad; recording starts simultaneously after the second choice. B1 or STOP cancels the first choice; occupied pads require confirmation. STOP finishes recording. Hold Pad Bank and use pads 1–8 for banks. Standard stereo recording on one pad remains in the normal Sample Record window; Routing is offered only for the Rear source.
- **Monitor without recording.** Input Config → Shift+B2 Routing: B2/B3 toggle L/R monitoring, K2/K3 set pan, Shift+K2/K3 set monitor level. The same controls work in recording Routing. Input Config pad presses never start recording. Monitor gain/pan do not change the dry mono recordings. Settings are saved with the project.
- **Knob FX priority.** Physical K1–K3 control Knob FX while it is open, including over custom screens. Routing resumes afterwards. Note Repeat keeps its controls.
- **Scale and Orig Note.** Setting Orig Note selects Scale. Tune lighting resolves the current sample from the project model, including when a different pad is selected in Compressor and across banks A–H.
- **Menu clicks.** A fast encoder click after leaving Mod Settings is passed to the current native menu. Hidden Sample controls no longer take Project controls.
- **Copy and EQ.** Hold Erase after entering Copy with Shift+Erase; Shift+Pad Bank switches banks backwards. Returning from Seq to Sample retains the EQ page.

**Recording limits:** custom mono/dual capture requires Rec Length **Free**, uses 24-bit / 44.1 kHz WAV and the native input threshold. It records the dry inputs. Stereo and the original Rear L / Rear R source screens remain native.

**Validation:** ARM64/UBSan covers recording selection, cancellation, overwrite confirmation, bank changes and FX/Repeat priority. Audio tests cover dry synchronized capture, monitor pan/level and cancellation. The full native preload constructor passes. The preceding r27o UI was exercised in VM; r27p was installed with all 77 files verified and accepted by the author on MPC. This does not close every historical report. FAT32 remains experimental.

**Install:** back up projects and samples. Copy `autorun/` to the card root or select this ZIP in Installer 1.0.3. Latest release now selects r27p.

[Known issues](KNOWN_ISSUES.md) · [Earlier r27n changes](RELEASE_NOTES-r27n.md). Thanks to Leusha, mxt1iga and everyone testing and reporting issues.

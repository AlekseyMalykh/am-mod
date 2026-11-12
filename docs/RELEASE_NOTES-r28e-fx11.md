# r28e-fx11 (candidate for r29a)

Fix: Save Project and Save Sample windows are SD-only. K3 is forced to External when the window opens and on any K3 movement. Diagnostic logging (SAVE_SCREEN/SAVE_K3/SAVE_BUTTON) is still enabled.

Master FX: the pinned "Sample Comp" row is hidden for now (the Sample compressor only exists after it has been opened once in Sample; Duck had no master view). Add OptoCat from the FX catalog instead.

Text input: naming, search and export windows (project, sample, track names in the Mixer, Stems export) now open in Pads mode by default. K1 still switches to Encoder. New Mod Setting: Quick tab > Text input (Pads/Encoder), saved on the card.


Fixed: Stems export failed with SD_EXPORT_UNAVAILABLE when the export destination was External (card Samples/User/Export); Stems always go to Samples/User/Export on the SD card, whichever destination the Export window shows, in a folder named after the song. First Mod Settings tab renamed Pads -> Quick.

Stems: the stock mixdown WAV is moved into the song folder as "<song>_Master.wav" instead of staying in Samples/Export. Pads default now also routes pad presses (previously it showed Pads but pads did not respond until K1 was toggled).

Stem file names now carry the Track name: "<song>_track A_<name>.wav" (only if the Track was renamed), "<song>_Master.wav".

- Effect screens: the value strip under the pictures is gone (values are on the parameter panel). New detailed pictures with AM MOD labels: Tape Delay (reels, rollers, heads), Tape (cassette with VU and transport), Vinyl (turntable), Sample Delay.

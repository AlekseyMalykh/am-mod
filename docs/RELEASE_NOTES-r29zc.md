# r29zc

- Pad lighting: the Am MOD palette now stays on while a native dialog (resample progress and other modal windows) or Sample Record covers the Sample screen. Previously the pads switched to the stock colours.
- Diagnostics: `INPUT_FX` lines in the console log show the recording route (point, track, Wet/Dry, which effect chains are ready) and the first input audio calls. Used to find why Record via Track/Master stays dry.
- Includes r29zb (sample resample keeps the attack).

# Am MOD r30b-usb14 — FX controls and tape transport

SD Card Access uses top B1/B3 captions for Cancel/Disconnect/Return. Its existing safe disconnect flow is unchanged.

Tape Delay paths are tangent to both tape packs and rollers and render beneath reel flanges. Both Tape Delay reels and both Tape cassette reels turn counterclockwise; bounded Wow cannot reverse transport as elapsed time grows.

Track pads 1-8, 15 (Input) and 16 (Master) leave an FX editor for that channel's FX list. Naming and delete-confirmation ownership remain separate.

Amp Sim: K1 Amp Model, K2 Drive, K3 Output; Shift Soft Clip/Channels/Bias. Tone page: Bass/Mid/Treble; Shift Top Boost/Mid Freq/Cab Model. B3 Solo and Shift+B3 Chain Bypass are restored. Added SV Bass, BM Round and GK Punch with bass cabinets and distinct hardware appearances. Existing guitar voice markers retain compatibility. These remain authored AIR Amp Sim voicings, not measured circuit models. Moving within the current model knob bucket preserves custom edits; reset follows the selected voice.

Delay, Diff Delay, Spring, Phaser and Flanger visuals have clearer scales/mechanical details. Detailed checks cover knob wiring, idle bypass, pad/release ownership, saved voicings, tangent geometry, flange occlusion and transport direction on host/sanitizers/AArch64. Pinned native DSP and full preload/USB paths are also verified; hardware listening is pending.

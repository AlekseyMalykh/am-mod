export const SAMPLE_TAB_GROUPS = [
  ["TRIM", "MIX", "AMP ENV"],
  ["TUNE", "PLAY", "TUNER"],
  ["FILTER", "FILT ENV"],
] as const;

export type SampleTab = (typeof SAMPLE_TAB_GROUPS)[number][number];

export const SHIFT_PAD_FUNCTIONS = [
  "FULL LEVEL",
  "HALF SEQ",
  "DOUBLE SEQ",
  "COUNT-IN",
  "COMPRESSOR",
  "HALF SPEED",
  "DOUBLE SPEED",
  "MIDI CONFIG",
  "FADER",
  "REC QUANTIZE",
  "RESAMPLE",
  "SONG",
  "TRIM SAMPLE",
  "TIME CORRECT",
  "WARP",
  "PROJECT",
] as const;

export const FADER_MAPPINGS = [
  "Off",
  "Pad Volume",
  "Pad Pan",
  "Pad Tune",
  "Pad Amp Attack",
  "Pad Amp Decay/Release",
  "Pad Filter Cutoff",
  "Kit Volume",
] as const;

export const FILTER_TYPES = ["OFF", "CLASSIC", "LPF2", "LPF4", "HPF2", "HPF4", "BPF2", "BPF4"] as const;
export const INPUT_SOURCES = ["MIC", "REAR", "REAR L", "REAR R", "RESAMPLE", "USB", "USB L", "USB R"] as const;
export const MONITOR_MODES = ["OFF", "AUTO", "ON"] as const;
export const Q_VALUES = ["1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64"] as const;

export const PAD_FX = [
  { name: "HALF SPEED", params: ["SPEED", "MIX", "-"] },
  { name: "CHORUS", params: ["RATE", "DEPTH", "FEEDBACK"] },
  { name: "FLANGER", params: ["RATE", "DEPTH", "FEEDBACK"] },
  { name: "PHASER", params: ["FEEDBACK", "SPEED", "RANGE"] },
  { name: "COMB FILTER", params: ["SPEED", "-", "-"] },
  { name: "LP FILTER", params: ["RESONANCE", "SPEED", "RANGE"] },
  { name: "HP FILTER", params: ["RESONANCE", "SPEED", "RANGE"] },
  { name: "BP FILTER", params: ["RESONANCE", "SPEED", "RANGE"] },
  { name: "RING MOD", params: ["MAX FREQ", "-", "-"] },
  { name: "LOFI", params: ["BITCRUSH", "DECIMATOR", "-"] },
  { name: "COLOR", params: ["MODE", "-", "-"] },
  { name: "GRANULATOR", params: ["DENSITY", "FEEDBACK", "GRAIN LEN"] },
  { name: "BEAT REPEAT", params: ["DIVISION", "REVERSE", "RESONANCE"] },
  { name: "REV STEPPER", params: ["DELAY TIME", "REPEATS", "-"] },
  { name: "DELAY", params: ["TIME", "FEEDBACK", "RANGE"] },
  { name: "REVERB", params: ["PRE-DELAY", "DECAY", "DIFFUSION"] },
] as const;

export const FLEX_BEAT_SLOTS = [
  "EMPTY", "STUTTER", "REPEATER", "BEAT CHOP", "HALF TIME", "REVERSE", "PITCH DOWN", "PITCH UP",
  "SCRATCH 1", "SCRATCH 2", "TRIP SCRATCH", "BAR SCRATCH", "TRANCE GT 1", "TRANCE GT 2", "TRANCE GT 3", "TRANCE GT 4",
] as const;

export const KNOB_FX = [
  "DELAY", "DIFF DELAY", "TAPE DELAY", "SAMPLE DELAY", "REVERB SMALL", "REVERB MEDIUM", "REVERB LARGE",
  "SPRING REVERB", "HP FILTER", "LP FILTER", "BP FILTER", "BUS COMPRESSOR", "LIMITER", "PUMPER", "TRANSIENT",
  "NOISE GATE", "AMP SIM", "TUBE DRIVE", "SOFT CLIPPER", "ENSEMBLE", "MULTI-CHORUS", "PHASER", "FLANGER",
  "AUTO-WAH", "AUTO-PAN", "VINTAGE EMULATOR", "VINYL EMULATOR", "TAPE EMULATOR",
] as const;

export const MIDI_SETTINGS = [
  ["MIDI PORT", "EXTERNAL"],
  ["MIDI IN CHANNEL", "ALL"],
  ["MIDI OUT CHANNEL", "1"],
  ["PAD MIDI IN", "ON"],
  ["PAD MIDI OUT", "ALWAYS"],
  ["MIDI SYNC IN", "OFF"],
  ["MIDI SYNC OUT", "OFF"],
  ["MIDI THRU", "OFF"],
  ["RECEIVE PROGRAM", "OFF"],
  ["CV/SYNC OUT", "OFF"],
  ["CV/SYNC BASE", "1"],
  ["CV/SYNC DIVISION", "1"],
  ["PARAM TAKEOVER", "SCALED"],
  ["KNOB FX TAKEOVER", "SCALED"],
  ["FADER TAKEOVER", "PICKUP"],
  ["USB POWER", "ON"],
  ["RESET FACTORY SETTINGS", ""],
  ["RESET FACTORY DATA", ""],
] as const;

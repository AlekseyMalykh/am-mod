import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

const root = new URL("../app/", import.meta.url);
const [model, workbench, css, globals] = await Promise.all([
  readFile(new URL("firmware-model.ts", root), "utf8"),
  readFile(new URL("mpc-workbench.tsx", root), "utf8"),
  readFile(new URL("ac50-states.css", root), "utf8"),
  readFile(new URL("globals.css", root), "utf8"),
]);

test("matches the firmware Sample Mode tab groups", () => {
  assert.match(model, /\["TRIM", "MIX", "AMP ENV"\]/);
  assert.match(model, /\["TUNE", "PLAY", "TUNER"\]/);
  assert.match(model, /\["FILTER", "FILT ENV"\]/);
  assert.match(workbench, /next\[index\]=\(next\[index\]\+1\)%SAMPLE_TAB_GROUPS\[index\]\.length;setSampleTabs\(next\)/);
});

test("keeps B buttons separate from K knobs", () => {
  assert.match(workbench, /aria-label="B1" onClick=\{\(\)=>functionButton\(0\)\}/);
  assert.match(workbench, /value=\{v\} onChange=\{n=>turnKnob\(i,n\)\}/);
  assert.doesNotMatch(workbench, /onPress=\{\(\)=>functionButton/);
});

test("contains all sixteen stock Shift+Pad functions", () => {
  for (const label of [
    "FULL LEVEL", "HALF SEQ", "DOUBLE SEQ", "COUNT-IN", "COMPRESSOR", "HALF SPEED", "DOUBLE SPEED", "MIDI CONFIG",
    "FADER", "REC QUANTIZE", "RESAMPLE", "SONG", "TRIM SAMPLE", "TIME CORRECT", "WARP", "PROJECT",
  ]) assert.match(model, new RegExp(`"${label}"`));
});

test("opens advanced 16 Levels setup only from the visible B2 Setup action", () => {
  assert.match(workbench, /const openLevelsConfig=.*setScreen\("levelsconfig"\)/);
  assert.match(workbench, /if\(index===1\)openLevelsConfig\(\)/);
  assert.doesNotMatch(workbench, /label="16 LEVELS"[\s\S]*?onHold=\{openLevelsConfig\}/);
  assert.doesNotMatch(workbench, /if\(shift\)openLevelsConfig\(\)/);
  assert.match(workbench, /\["TYPE",levelsType\]/);
  assert.doesNotMatch(workbench, /setTimeout\([^)]*levelsconfig/);
});

test("lights the physical key that was pressed in chromatic and scale layouts", () => {
  assert.match(workbench, /const fire=\(pad:number,level=1,tune=0,lightPad=pad,slice=-1\)/);
  assert.match(workbench, /fire\(selectedPad,level,tune,pad\)/);
  assert.match(workbench, /x!==lightPad/);
});

test("step grid has a latched sample selector and independent playhead lighting", () => {
  assert.match(workbench, /stepSampleSelect,setStepSampleSelect/);
  assert.match(workbench, /SELECT STEP SOUND/);
  assert.match(workbench, /stepOn\?"step-on"/);
  assert.match(workbench, /playhead\?"step-playhead"/);
});

test("moves complete step events and supports note and chop entry", () => {
  assert.match(workbench, /const moveStepEvent=/);
  assert.match(workbench, /EVENT \$\{from\+1\} → \$\{to\+1\}/);
  assert.match(workbench, /stepInputMode==="levels"/);
  assert.match(workbench, /stepInputMode==="chop"/);
  assert.match(workbench, /stateRef\.current\.stepTunes/);
  assert.match(workbench, /stateRef\.current\.stepSlices/);
});

test("selects Sample, Chop or 16 Levels from B1 Source", () => {
  assert.match(workbench, /STEP_SOURCE_ORDER/);
  assert.match(workbench, /stepSourcePicker,setStepSourcePicker/);
  assert.match(workbench, /cycleStepSource\(1\)/);
  assert.match(workbench, /adjustStepSource\(delta\)/);
  assert.match(workbench, /adjustStepSource\(-1\)/);
  assert.match(workbench, /adjustStepSource\(1\)/);
  assert.match(workbench, /writeStepSourceEvent\(step\)/);
  assert.match(workbench, /<b>SAMPLE<\/b><span>NEXT TYPE<\/span><em>DONE<\/em>/);
  assert.match(workbench, /screen==="step"&&stepSourcePicker\)\{setStepSource\("chop"\)/);
  assert.match(workbench, /screen==="step"&&stepSourcePicker\)\{setStepSource\("note"\)/);
  assert.match(workbench, /PRESS DATA → STEP GRID/);
});

test("defaults the step grid to row order and exposes alternatives in MIDI Config", () => {
  assert.match(workbench, /useState<StepDirection>\("ROW"\)/);
  assert.match(workbench, /\["STEP ORDER",stepDirection\]/);
  assert.match(workbench, /stepDirection==="PAD NUM"\?pad:stepDirection==="ROW"\?visual/);
});

test("offers USB power without a battery-draining off state", () => {
  assert.match(model, /\["USB POWER", "ON"\]/);
  assert.match(workbench, /index===15\?\["ON","LOW BATTERY"\]/);
  assert.doesNotMatch(workbench, /\["ON","LOW BATTERY","OFF"\]/);
});

test("data encoder moves the persistent K cursor and briefly reveals values", () => {
  assert.match(workbench, /const cycleEncoderKnob=\(backwards=false\)/);
  assert.match(workbench, /cycleEncoderKnob\(shift\)/);
  assert.match(workbench, /encoderKnobFocus===null\?\(backwards\?2:0\)/);
  assert.match(workbench, /\+3\)%3/);
  assert.doesNotMatch(workbench, /DATA: NAV/);
  assert.match(workbench, /setEncoderKnobFocus\(index\).*setPeekKnob\(index\)/);
  assert.match(workbench, /className="knob-focus-cursor"/);
  assert.match(workbench, /setTimeout\(\(\)=>setPeekKnob\(null\),1000\)/);
  assert.match(workbench, /className="encoder-value-peek"/);
  assert.match(workbench, /knobPercent\(encoderKnobFocus\)\+delta/);
  assert.match(css, /\.knob-focus-cursor\s*\{/);
  assert.match(css, /border-top:\s*0/);
});

test("uses the same forward and reverse K focus cycle in Step and Sample", () => {
  assert.match(workbench, /encoderKnobFocus===null\?\(backwards\?2:0\)/);
  assert.match(workbench, /encoderKnobFocus\+\(backwards\?-1:1\)\+3\)%3/);
  assert.doesNotMatch(workbench, /DATA: STEP POSITION/);
  assert.doesNotMatch(workbench, /\[null,0,1,2\]/);
  assert.match(workbench, /screen==="step"&&stepSourcePicker\)\{setStepSourcePicker\(false\)/);
  assert.doesNotMatch(workbench, /if\(screen==="step"\)\{setStepSourcePicker\(v=>!v\)/);
  assert.match(workbench, /if\(index===0\)\{setStepSourcePicker\(true\)/);
});

test("keeps SEQ as an overview and gives Step Edit grid and event views", () => {
  assert.match(workbench, /className="seq-overview"/);
  assert.match(workbench, /className="step-events"/);
  assert.match(workbench, /event-kind kind-/);
  assert.match(workbench, /setStepView\(v=>v==="list"\?"grid":"list"\)/);
  assert.match(workbench, /if\(target==="step"\)setStepView\("grid"\)/);
  assert.match(workbench, /stepPosition\(position\)/);
});

test("assigns B3 to contextual event erase even before the Step cursor moves", () => {
  assert.match(workbench, /if\(index===2\)\{const position=current>=0\?current:page\*STEPS_PER_BAR/);
  assert.match(workbench, /clearPadEvents\(selectedPad,position\)/);
  assert.match(workbench, /NO \$\{padAddress\(selectedPad\)\} EVENT AT STEP/);
  assert.doesNotMatch(workbench, /index===2&&current>=0/);
  assert.match(workbench, /step-b-actions"><b>SOURCE<\/b><span>\{shift\?"MOVE EVT":"EVENTS"\}<\/span><em>ERASE<\/em>/);
  assert.match(workbench, /className="step-bar-row"/);
  assert.match(workbench, /<span>BAR \{page\+1\}\/\{sequenceBars\}<\/span>/);
});

test("matches the stock 1–128 bar range and discrete K1 choices", () => {
  assert.match(workbench, /const MAX_SEQUENCE_BARS=128/);
  assert.match(workbench, /const MAX_SEQUENCE_STEPS=STEPS_PER_BAR\*MAX_SEQUENCE_BARS/);
  assert.match(workbench, /const SEQUENCE_BAR_CHOICES=\[1,2,4,8,16,32,64,128\]/);
  assert.match(workbench, /const adjustSequenceKnob=/);
  assert.match(workbench, /SEQUENCE_LENGTHS\[Math\.max\(0,Math\.min\(SEQUENCE_LENGTHS\.length-1,position\+delta\)\)\]/);
  assert.match(workbench, /sequenceBars\+delta/);
  assert.match(workbench, /"BARS"/);
  assert.match(workbench, /EXACT BARS/);
  assert.match(workbench, /adjustSequenceKnob\(encoderKnobFocus,delta\)/);
});

test("distinguishes live sequence recording from step recording", () => {
  assert.match(workbench, /recordStyle,setRecordStyle/);
  assert.match(workbench, /setRecordStyle\("step"\)/);
  assert.match(workbench, /setRecordStyle\("live"\)/);
  assert.match(workbench, /LIVE REC/);
  assert.match(workbench, /STEP REC/);
});

test("addresses samples across eight banks and supports quick bank selection", () => {
  assert.match(workbench, /const SAMPLE_SLOTS=128/);
  assert.match(workbench, /const sampleSlot=\(pad:number\)=>bank\*16\+pad/);
  assert.match(workbench, /stepSource==="sample"\)setBankOverlay\("sample"\)/);
  assert.match(workbench, /SOURCE SAMPLE BANK/);
  assert.match(workbench, /PADS 1–8/);
  assert.match(workbench, /const BANK_DISPLAY_ORDER=\[4,5,6,7,0,1,2,3\]/);
  assert.match(workbench, /TOP E–H · BOTTOM A–D/);
  assert.match(workbench, /quickBankActive=quickBank&&/);
  assert.match(workbench, /bankDim=quickBankOverlay&&p>=8/);
  assert.match(css, /physical-pad\.bank-choice-active\{border-color:#ff425b/);
  assert.match(css, /physical-pad\.bank-dim\{/);
});

test("limits Step Bank navigation to sequence bars and uses a distinct color", () => {
  assert.match(workbench, /wrap\(page,sequenceBars\)/);
  assert.match(workbench, /target<sequenceBars/);
  assert.match(workbench, /bankBar<sequenceBars/);
  assert.match(workbench, /STEP BANK/);
  assert.match(workbench, /step-bank-choice/);
  assert.match(css, /physical-pad\.step-bank-choice\{border-color:#7658e8/);
  assert.match(css, /physical-pad\.bank-unavailable/);
});

test("clears the momentary Pad Bank overlay on release and mode changes", () => {
  assert.match(workbench, /setMode=.*setBankOverlay\(null\)/);
  assert.match(workbench, /label="PAD BANK"[\s\S]*?onPressEnd=\{\(\)=>setBankOverlay\(null\)\}/);
});

test("lets Step Edit program empty sound slots and shows the active bar range", () => {
  assert.doesNotMatch(workbench, /EMPTY \$\{padAddress\(selectedPad\)\} · LOAD OR RECORD A SAMPLE/);
  assert.match(workbench, /SOUND EMPTY/);
  assert.match(workbench, /className="step-bar-map"/);
  assert.match(workbench, /bar<sequenceBars\?"available"/);
  assert.match(workbench, /bar===page\?"current"/);
  assert.match(css, /\.step-bar-map\s*\{/);
});

test("extends chop, resampling and tuning workflows", () => {
  assert.match(workbench, /chopPage,setChopPage/);
  assert.match(workbench, /extractSelected/);
  assert.match(workbench, /resampleStage,setResampleStage/);
  assert.match(workbench, /resampleWithFx,setResampleWithFx/);
  assert.match(workbench, /resampleDestination,setResampleDestination/);
  assert.match(workbench, /<small>SOURCE<\/small>/);
  assert.match(workbench, /<small>DEST<\/small>/);
  assert.match(workbench, /<small>PROCESS<\/small>/);
  assert.match(workbench, /<b>CANCEL<\/b><span>PREVIEW<\/span><em>DO<\/em>/);
  assert.match(workbench, /detectPitch/);
  assert.match(workbench, /INPUT TUNER/);
  assert.match(workbench, /stereo-wave/);
});

test("keeps B actions above and K parameters below in Chop", () => {
  assert.match(workbench, /className="lcd-tabs chop-b-actions"/);
  assert.match(workbench, /shift\?"EXTRACT ONE":"EXTRACT GROUP"/);
  assert.match(workbench, /className="lcd-values sample-knob-labels chop-k-params"/);
  assert.match(workbench, /shift\?"THRESHOLD":"CHOP TYPE"/);
});

test("matches the stock Shift+Pad menu layouts and controls", () => {
  assert.match(workbench, /FADER_CHOICES=FADER_MAPPINGS\.filter\(value=>value!=="Off"\)/);
  assert.match(workbench, /<span>ATTACK<\/span>/);
  assert.match(workbench, /<span>RELEASE<\/span>/);
  assert.match(workbench, /shift\?"IN BOOST":"AMOUNT"/);
  assert.match(workbench, /className="tc-pad-map"/);
  assert.match(workbench, /<b>CANCEL<\/b><span>ALL<\/span><em>DO IT!<\/em>/);
  assert.match(workbench, /<b>EXPORT<\/b><span>REMOVE<\/span><em>INSERT<\/em>/);
  assert.match(workbench, /midiEdit\?"TURN VALUE":"TURN SELECT"/);
  assert.match(workbench, /\["LOAD PROJECT","SAVE PROJECT","NEW PROJECT","SD CARD ACCESS"\]/);
});

test("starts with silent empty pads and never synthesizes missing samples", () => {
  assert.match(workbench, /Array\(SAMPLE_SLOTS\)\.fill\("EMPTY"\)/);
  assert.match(workbench, /if\(!sample\)\{setMessage\(`EMPTY \$\{padAddress\(pad\)\}`\);return\}/);
  assert.doesNotMatch(workbench, /createOscillator/);
  assert.match(workbench, /EVENT NOT RECORDED/);
});

test("models held Erase and latched Note Repeat gestures", () => {
  assert.match(workbench, /const beginEraseHold=/);
  assert.match(workbench, /const erasePadGesture=/);
  assert.match(workbench, /if\(screen==="step"\)\{clearPadEvents\(slot\)/);
  assert.match(workbench, /noteRepeatHeld\|\|noteRepeatLatched/);
  assert.match(workbench, /\["1\/8T","1\/16T","1\/32T"\]/);
  assert.match(workbench, /noteRepeatMilliseconds/);
});

test("supports multi-key right-click latching until the next left click", () => {
  assert.match(workbench, /onContextMenu=\{event=>\{event\.preventDefault\(\)/);
  assert.match(workbench, /if\(latchedRef\.current\)\{latchedRef\.current=false;setLatched\(false\);\(onLatchEnd\?\?onPressEnd\)\?\.\(\);return\}/);
  assert.match(workbench, /document\.addEventListener\("click",release\)/);
  assert.match(css, /hw-key\.is-held/);
});

test("renders two five-segment meters and the physical two-row bank indicator", () => {
  assert.match(workbench, /aria-label="Stereo level meter"/);
  assert.match(workbench, /Array\.from\(\{length:10\}/);
  assert.match(workbench, /BANK_DISPLAY_ORDER\.map/);
  assert.match(css, /bank-indicator\{[^}]*grid-template-columns:repeat\(4,1fr\)[^}]*grid-template-rows:repeat\(2,18px\)/);
  assert.match(css, /\.vu\{[^}]*top:73px;height:130px[^}]*grid-template-columns:repeat\(2,8px\)[^}]*grid-template-rows:repeat\(5,1fr\)/);
});

test("keeps 16 Levels B actions and K parameters in separate rows", () => {
  assert.match(workbench, /className="lcd-tabs levels-b-actions"><b>SOURCE<\/b><span>SETUP<\/span><em>EXIT<\/em>/);
  assert.match(workbench, /className="knob-strip levels-k-params"/);
  assert.match(workbench, /className="levels-config-list"/);
  assert.match(css, /levels-config-list\{[^}]*flex-direction:column/);
  assert.match(workbench, /layoutMode==="CHROMATIC"\?\[\["ROOT"/);
});

test("adds input and sample tuners without consuming K controls", () => {
  assert.match(model, /\["TUNE", "PLAY", "TUNER"\]/);
  assert.match(workbench, /if\(screen==="record"\).*index===1\)void openTuner\("input","record"\)/);
  assert.match(workbench, /<b>MONITOR<\/b><span>TUNER<\/span><em>—<\/em>/);
  assert.match(workbench, /currentSampleTab==="TUNER"/);
  assert.match(workbench, /className="tuner-note sample-tuner"/);
});

test("preserves complete events in Shift+Pad sequence transformations", () => {
  assert.match(workbench, /const transformSequence=/);
  assert.match(workbench, /setStepTunes\(remap\(stepTunes,0\)\)/);
  assert.match(workbench, /setStepSlices\(remap\(stepSlices,-1\)\)/);
  assert.match(workbench, /setStepKinds\(remap\(stepKinds,"sample" as StepKind\)\)/);
  assert.match(workbench, /setStepVelocities\(remap\(stepVelocities,100\)\)/);
});

test("applies Time Correct only to selected pads with all event metadata", () => {
  assert.match(workbench, /const applyTimeCorrect=/);
  assert.match(workbench, /timeCorrectPads\[pad%16\]/);
  assert.match(workbench, /setTimeCorrectPads\(Array\(16\)\.fill\(true\)\)/);
  assert.match(workbench, /setStepTunes\(remap\(stepTunes,0\)\)/);
  assert.match(workbench, /setStepSlices\(remap\(stepSlices,-1\)\)/);
  assert.match(workbench, /setStepKinds\(remap\(stepKinds,"sample" as StepKind\)\)/);
  assert.match(workbench, /setStepVelocities\(remap\(stepVelocities,100\)\)/);
});

test("keeps Chop slice editing and stock Shift actions distinct", () => {
  assert.match(workbench, /\[chopType,setChopType\]=useState\(0\)/);
  assert.match(workbench, /const buildChopPoints=/);
  assert.match(workbench, /editChopBoundary\("start",value\)/);
  assert.match(workbench, /editChopBoundary\("end",value\)/);
  assert.match(workbench, /if\(shift\)\{if\(index===0\)extractChops\(\[currentChop\],null\);if\(index===1\)splitCurrentChop\(\);if\(index===2\)mergeCurrentChop\(\)\}/);
  assert.match(workbench, /shift\?"THRESHOLD":"CHOP TYPE"/);
  assert.match(workbench, /slice>=chopCount\?"chop-unavailable"/);
});

test("uses perimeter-only lighting for Chop and step states", async () => {
  const styles = await readFile(new URL("ac50-states.css", root), "utf8");
  assert.match(styles, /AC50 pads are opaque rubber; RGB light is visible around the perimeter only/);
  assert.match(styles, /physical-pad\.chop-current/);
  assert.match(styles, /physical-pad\.chop-unavailable/);
  assert.match(styles, /physical-pad:nth-last-child\(-n\+4\) \{ border-color: #168fc2/);
  assert.match(styles, /physical-pad\.step-on \{ border-color:/);
  assert.match(styles, /physical-pad\.step-other \{ border-color: #9a63e8/);
  assert.match(styles, /physical-pad\.step-playhead \{ border-color: #ff4050/);
  assert.match(styles, /physical-pad\.step-unavailable \{ border-color:/);
  assert.match(styles, /physical-pad\.step-playhead \{ border-color:/);
});

test("opens the running step sequencer grid and follows its playhead pages", () => {
  assert.match(workbench, /if\(!playing&&screen==="step"\)\{setStepView\("grid"\)/);
  assert.match(workbench, /setMessage\("STEP PLAY · FOLLOW"\)/);
  assert.match(workbench, /if\(screen==="step"\)setPage\(Math\.floor\(s\/16\)\)/);
  assert.match(workbench, /stepAvailable=isStep&&step<sequenceLength/);
  assert.match(workbench, /stepOther=stepAvailable&&!stepOn&&eventsAtStep\(step\)/);
  assert.match(workbench, /playhead=stepAvailable&&current===step/);
});

test("keeps printed pad legends immutable and changes only lighting classes", () => {
  assert.match(workbench, /<span>\{p\+1\}<\/span><small>\{SHIFT_PAD_FUNCTIONS\[p\]\}<\/small>/);
  assert.doesNotMatch(workbench, /<span>\{isStep\?/);
  assert.doesNotMatch(workbench, /<small>\{label\}<\/small>/);
});

test("uses bank wrapping, keyboard-style octave switching, and precise nudging", () => {
  assert.match(workbench, /const delta=shift\?-1:1,wrap=/);
  assert.match(workbench, /wrap\(bank,8\)/);
  assert.match(workbench, /wrap\(chopPage,pages\)/);
  assert.match(workbench, /const NOTE_OCTAVE_MIN=-4,NOTE_OCTAVE_MAX=4/);
  assert.match(workbench, /noteOctave\+delta/);
  assert.match(workbench, /octaveOffset=noteOctave\*12/);
  assert.match(workbench, /setMessage\(`OCTAVE/);
  assert.doesNotMatch(workbench, /wrap\(noteOctave/);
  assert.match(workbench, /const nudgeStep=/);
  assert.match(workbench, /stepSourcePicker\)adjustStepSource\(-1\)/);
  assert.match(workbench, /stepSourcePicker\)adjustStepSource\(1\)/);
  assert.doesNotMatch(workbench, /stepView==="list".*selectStepEvent/);
});

test("aligns B2, LCD, K2 and pads on one hardware centre line", () => {
  assert.match(globals, /display-stack\{position:absolute;left:50%/);
  assert.match(globals, /grid-template-columns:142px repeat\(3,1fr\) 142px/);
  assert.match(globals, /lower-grid\{grid-template-columns:142px 1fr 142px/);
  assert.match(globals, /hw-key\{height:29px;min-height:29px/);
  assert.match(css, /soft-keys\{display:grid;grid-template-columns:repeat\(3,minmax\(0,1fr\)\)/);
  assert.match(css, /lcd-tabs\{display:grid;grid-template-columns:repeat\(3,minmax\(0,1fr\)\)/);
});

test("keeps physical K labels fixed and rotates the endless data encoder", () => {
  assert.match(workbench, /label=\{`K\$\{i\+1\}`\}/);
  assert.doesNotMatch(workbench, /encoderKnobFocus===i\?" •"/);
  assert.match(workbench, /dataAngle,setDataAngle/);
  assert.match(workbench, /setDataAngle\(v=>v\+delta\*18\)/);
  assert.match(globals, /transform:rotate\(var\(--data-turn,0deg\)\)/);
});

test("separates the data encoder turn ring from its centre press button", () => {
  assert.match(workbench, /className="data-wheel"[\s\S]*aria-label="Data encoder ring: scroll to turn"[\s\S]*onWheel=/);
  assert.match(workbench, /className="data-wheel-press" aria-label="Data encoder: press"/);
  assert.match(workbench, /onWheel=\{e=>e\.stopPropagation\(\)\} onClick=\{encoderPress\}/);
  assert.match(globals, /\.data-wheel-press\{/);
  assert.match(globals, /\.data-wheel-press:active\{/);
});

test("stops every active sample voice on a double Stop press", () => {
  assert.match(workbench, /activeSources=useRef<Set<AudioBufferSourceNode>>/);
  assert.match(workbench, /activeSources\.current\.add\(src\)/);
  assert.match(workbench, /src\.onended=\(\)=>activeSources\.current\.delete\(src\)/);
  assert.match(workbench, /const stopAllSounds=\(\)=>\{for\(const source of activeSources\.current\)try\{source\.stop\(\)\}/);
  assert.match(workbench, /doublePress=now-stopPressTime\.current<350/);
  assert.match(workbench, /sub="DOUBLE: ALL SOUNDS" onClick=\{transportStop\}/);
});

test("renders real channel envelopes and maps Shift plus K to waveform zoom", () => {
  assert.match(workbench, /const waveformEnvelope=/);
  assert.match(workbench, /sample\.getChannelData\(Math\.min\(channel,sample\.numberOfChannels-1\)\)/);
  assert.match(workbench, /stereoWaveform=\[waveformEnvelope\(0,waveViewStart,waveViewEnd\),waveformEnvelope\(1,waveViewStart,waveViewEnd\)\]/);
  assert.match(workbench, /channel===0\?"L":buffers\.current\[selectedPad\]\?\.numberOfChannels===1\?"M":"R"/);
  assert.match(workbench, /if\(shift\)\{const zoom=1\+value\/100\*31,anchor:WaveAnchor=index===0\?"start":index===1\?"end":"selection"/);
  assert.match(workbench, /\["ZOOM START"/);
  assert.match(workbench, /\["ZOOM END"/);
  assert.match(workbench, /\["ZOOM ALL"/);
  assert.match(workbench, /label="SHIFT" onClick=\{\(\)=>\{\}\} onPressStart=\{\(\)=>setShift\(true\)\} onPressEnd=\{\(\)=>setShift\(false\)\}/);
  assert.match(css, /\.wave-bars>i\.hot/);
});

test("keeps MPC SAMPLE on one line above a square speaker", () => {
  assert.match(globals, /product-name\{right:18px;top:17px;max-width:none;white-space:nowrap/);
  assert.match(globals, /speaker\{position:absolute;right:18px;top:58px;bottom:auto;width:104px;height:104px/);
});

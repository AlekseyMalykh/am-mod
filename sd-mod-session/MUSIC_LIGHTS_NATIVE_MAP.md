# r12a pinned native paths — MPC Sample 1.3.0.12

Stock SHA256 remains pinned by build.py. These are static ABI observations;
the new r12a refresh/navigation paths are not yet hardware verified.

- Encoder press: note `0x6f` dispatches through `0x159f330` to the BooleanProperty
  at input controller `+0x2d50`. Binding `0x1659848` installs `0x16541a0`, which
  invokes native forward/backward knob focus based on Shift at `+0x22a0`.
  r12a consumes the complete down/release gesture only for its custom page.
- Whole-layer refresh scheduling: constructor sequence
  `0x153ad90..0x153adf4` increments layer reference count `+0x48`, locks
  queue owner `+0x220`, and submits the 32-byte function via `0x2b59300` to
  queue owner `+0x1d8`. Function payload: invoke `0x1538cd0`, manage `0x1538b10`,
  captures `{layer+0x40, layer}`; cancellation token is aligned `layer+0x40`.
- Callback `0x1538cd0` calls `0x1534d30(layer)` and releases the same reference.
  This preserves native layer order; r12a's existing `0x1545930` wrapper supplies
  musical colours within SixteenLevelLighting, while naming keyboard keeps its
  existing final-sender override.
- SixteenLevelLighting constructor `0x1545a50` and destructor `0x1547b70`
  register/clear a bounded 8-entry pointer registry. Native destructor
  `0x1547d44..0x1547d6c` drains/waits for outstanding references before base
  destruction. A library mutex protects only acquiring that lifetime reference;
  it is released before locking the native queue.
- UI Timer compares the packed music snapshot every50ms and submits the latest
  changed snapshot once per active Tune layer. No extra thread, native-property
  toggle, file write, direct UI RefreshAll or direct UI MIDI output is added.
- Wrong native class, inactive 16 Levels or a type other than Tune skips requests.
  Queue rejection releases the reference and retries on a later UI tick.
- UI option bars use one segment per choice, with a single highlighted segment.
  Transpose16 has49 positions (−24…+24), with an extended centre marker at0.

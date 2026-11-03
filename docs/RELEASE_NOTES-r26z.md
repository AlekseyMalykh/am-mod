# r26z — physical splash display trial

Unpublished local candidate. Retains r26y and its exact application library.
Back up projects and samples before testing.

The r26y device trial showed a black screen. Its journal confirms a successful
frame write, which did not establish physical visibility. The same journal
confirms one native SD Load, no late autoload and `errors=0`.

- Perform a full framebuffer blank/unblank cycle before writing the owned
  Am MOD frame, matching the public SampQOL launcher's display-reset sequence.
- Use sequential writes with explicit row offsets. Treat reset/write failures
  as errors and record the framebuffer mode, channels, stride and failing stage.
- Log `FRAME_WRITTEN` rather than asserting that the physical screen showed it.
- Start the early recovery watcher via setsid, with a readiness acknowledgement
  before stopping stock. Keep a systemd fallback when setsid is unavailable.

ARM/QEMU/UBSan cover reset ordering/error handling, image bounds, early recovery
and unchanged first-Load behavior. Physical splash visibility and startup timing
require another MPC trial. See [device test details (Russian)](RELEASE_NOTES-r26z.ru.md).

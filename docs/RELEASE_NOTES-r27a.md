# r27a — DRM splash display trial

Unpublished local candidate. The application library is byte-identical to
r26y/r26z; OptoCat/Warm, projects, settings, pad colours and Steps are retained.
Back up samples/projects before testing.

On the physical MPC, r26z successfully wrote the 320×240 st7796sdrmfb framebuffer
and completed blank/unblank, but Am MOD stayed invisible. This candidate selects
an owned DRM framebuffer on the connected 320×240 panel and verifies active
scanout with GETCRTC. It drops DRM master before acknowledging readiness, keeps
only its own buffer alive, and releases it when MPC presents its replacement.
Repeated starts are idempotent; cancellation/errors/timeouts clean up owned resources.

ARM/QEMU/UBSan cover resource bounds, ownership, failure unwind and protected
cleanup. Native VM QMP output exactly matches the owned image's RGB pixels;
the native app takes over successfully. Repeated start and controller stop also pass.
**Physical MPC splash visibility still requires a device trial.**

[Device test details (Russian)](RELEASE_NOTES-r27a.ru.md).

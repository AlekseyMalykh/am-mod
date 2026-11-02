# r26p candidate — compressor lifecycle and shared Bypass

[Русский](RELEASE_NOTES-r26p.ru.md)

Fix the r26o master lifetime error: a replacement Colour processor can bind
again after New Project. The hook table is initialized once and retained;
old callbacks use native processing and late old destruction cannot clear the
new owner. UI handoff waits for quiescence by retrying, without blocking audio.
Type and OptoCat settings remain in RAM across master replacement.

B3 now uses the native Bypass parameter for Duck and OptoCat together. Bypass
stops both; wake enables only the type displayed on the page. B2 browsing while
bypassed does not enable processing. Shift+Pad5 opens the enabled type, or Duck
with Bypass active when both are off. Warm remains stored during Bypass.
OptoCat-specific settings remain session-only; native Bypass follows the native
project state. All r26o Warm, graphics, meter, gain cap, K and colour fixes remain.

ARM/QEMU tests execute real pinned Colour factory/process/destruction, four
master replacements, retained-table and old-callback cases, an in-flight handoff
rejection followed by a successful retry, shared Bypass/native parameter updates
and page opening policy. Normal and UBSan runs preserve audio/gain/Warm checks.
Component/event boundaries are fixtures; full native app and physical MPC
New Project/Shift+Pad5/B3 testing remain pending. Local candidate, unpublished.

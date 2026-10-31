# AM Mod r25x — 1 November 2026

Based on the r25w functionality confirmed by the author on a real MPC Sample.
This release removes temporary Erase-route and project-open callback diagnostics,
updates public documentation, and removes local debug paths from the distributed library.
The unnamed-storage diagnostic was already absent from the approved source.

Changes since public r24x:
- Per-pad Pad Colour: Sample → B1 Mix → Shift+K1; encoder press toggles colour focus.
- Erase deletes projects with their companion data; folder deletion asks B1 Cancel / B3 Delete.
- A single Mod-Exit-Last.xpj + Mod-Exit-Last_[ProjectData] exit copy.
- On startup, open the newest SD project in Projects or one folder below it.
- Correct Internal/External labels; Internal remains temporary.
- Monochrome / Piano / Scale Lights; older Tonic/Degrees migrate to Scale.
- Framed Shift badges, right-aligned Zoom: Loop lock, Seq Colour swatch, pad-entry fixes.

Firmware: MPC Sample 1.3.0.12 only. Save work to External.
Mod Off saves the exit copy before returning to the stock application.

The r25w device report and r25x automated release checks have different scopes.
The automated fixtures capture some native audio/UI receivers; they do not certify
all hardware combinations. See [the release verification record](RELEASE-VERIFICATION-r25x.json)
and the SHA256SUMS release asset.

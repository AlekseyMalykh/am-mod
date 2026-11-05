# Am MOD r27q — Degrees, search and window polish

> Candidate, not published. Sources pass a host build (gcc, `-Werror`) and host tests; the ARM build, VM run and MPC check are still pending.

MPC Sample **1.3.0.12**. Changes since r27p; includes all its features and fixes.

- **Degrees layout.** Sample → Tune → Play → Lights: K1 is now called **Layout** and offers Monochrome, Piano, Scale and **Degrees**. In Degrees the 16 pads play only notes of the key, one degree per pad (C Major: C D E F G A B C…); Transpose16 shifts by degrees. All pads are lit: the sample's original note (when it is in the key), the tonic I, and degrees IV and V each have their own colour (defaults Magenta, Green, Cyan, Yellow); other degrees are dimmer. Colours change with Shift+K2/K3 (groups I Tonic, IV, V, Key notes, Original). A project saved with Degrees opens as Scale in r27p and earlier; a changed V colour saves the project as MPCS18, which r27p and earlier cannot read.
- **Search.** The first three matches show directly under the query, with the count in the top right. Preview is on by default. K2 **Search in**: Folder / Library / SD Card / All replaces Current / Internal / SD Card (Internal searched only the mod's temporary memory); K3 sets the preview volume. With Encoder input, +/− move the cursor and Erase deletes the selected letter — also in the Save name window. Shift+Erase clears the query; the previous query returns the next time Search opens. The list shows five rows with the selected row highlighted and its folder below. Stock K1–K3 panels with value bars.
- **Rear Routing.** Stock-style window: B2/B3 Mon L / Mon R with an amber mark when monitoring is on; per channel the record pad, meter, pan and level; K1–K3 panels with the S mark at the bottom. The replace confirmation uses the stock frame.
- **Mod Settings.** Stock list: label left, value right, grey selected row. B2/B3 show the neighbouring tabs, page dots sit next to the title. Restore recommended looks like an action and confirms in the stock frame. Clearer footer hints.
- **Splash.** The original “Am” lettering is kept; “MOD” is redrawn at the same height on the same baseline. Version and repository address use anti-aliased text instead of the 8×8 pixel font.
- **Messages.** Technical “… unavailable” errors are replaced by short hints on what to do. Encoder hints read Turn / Press everywhere.

**Unchanged:** Merge in Chop is not restored — Erase + pad gives the same result.

[Known issues](KNOWN_ISSUES.md) · [r27p changes](RELEASE_NOTES-r27p.md)

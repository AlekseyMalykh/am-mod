# r26y — local candidate

Unpublished; the public release remains r26s. Back up samples and projects first.

- **Mod Settings** replaces Pad Lighting: four scrolling tabs, B2/B3 navigation,
  and Restore recommended last in each section. Reset requires confirmation:
  B1 Cancel / B3 Restore.
- Independent waveform/spectrum pad tint; native curves, envelopes and side
  strips retained. Recommended defaults keep original colours.
- Service/user palette swaps retain five user slots, update names and preserve
  stable XPJ assignment IDs. Working-pad cursor: Steady / Slow / Normal / Fast.
- XPJ snapshots include display preferences and OptoCat Type/parameters/Warm;
  native Duck/Bypass serialization is retained. Failed reads preserve current
  state; legacy projects still load.
- Normal-power recovery copies retain original identity across asynchronous
  serialization. Restoring preserves the recovery sample directory and adds
  a separate first-Save confirmation. Native autosave retains Temp-Project;
  it is never redirected to overwrite the original.
- Project Mod Off removed; normal-power recovery saving and emergency stock
  recovery retained. Early **Am MOD** splash before application preparation,
  with an independent preparation-failure recovery service.
- Select the SD project in the first native full Load; disable late duplicate
  autoload. Retain native Temp-Project autosave and subsequent manual loads.
- Steps keeps SEQ RECORD WRITE and adds SAMPLE RECORD TOGGLE. Modes and their
  LEDs are mutually exclusive.
- Retains K, OptoCat/Warm, Note Repeat, Standard Piano/Scale, EQ, workflow
  colours, browser ERASE, exFAT and experimental FAT32.

[Settings/tests](MOD_SETTINGS.md) · [Readiness](RELEASE_READINESS-r26y.md).
Selection still uses the newest saved XPJ on the card. Physical MPC timing
has not been measured. [Boot flow and device checks (Russian)](BOOT_FLOW.ru.md).

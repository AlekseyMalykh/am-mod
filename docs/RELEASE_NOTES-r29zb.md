# r29zb

- Fix (Am MOD): sample resample kept only the tail of the sound. The stock primary file held just the late part of the render (peak ~0.02) and was preferred over the Mixer's finished file; the Mixer result now always replaces it. Found in the r29za console log and the files on the card: the Mixer file had the full attack, the imported sample started 250 ms late.
- Includes r29za: Project menu without SD Card Access; Record rows greyed out in Stock; `RESAMPLE_MIX HEAD` diagnostic lines.

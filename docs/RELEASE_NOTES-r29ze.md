# r29ze

- Fix (Am MOD recording): Track and Master effects never reached the recording. After the first change of an effect chain the new program was waiting to be adopted by the audio thread, but only the lane that was already in use was ever adopted; the Track and Master lanes stayed "waiting" for ever and the route never switched. Idle lanes now adopt new programs on silence, so the route becomes ready. Found in the r29zd console log (`INPUT_FX route ... ready=0 result=0/0/2`).
- Diagnostics: the `INPUT_FX route` line now lists the effect count of every Track A-H.
- Includes r29zd (free pad renders at once, occupied pad asks).

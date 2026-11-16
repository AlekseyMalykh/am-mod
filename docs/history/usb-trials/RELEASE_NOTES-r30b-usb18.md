# Am MOD r30b-usb18

- Send To choices omit destinations forming direct or transitive feedback cycles, including grouped sends. Route/Mixer DATA, K2, scales and takeover share the same choices. Stored routing validation remains; Sidechain detector choices stay independent.
- Duck and OptoCat initially select K1. DATA cycles K1/K2/K3 forward; Shift+DATA cycles backward. Duck uses the same amber top edge and cleared cell frames as OptoCat.
- User confirmed the previous tuner/FX changes removed audible clicks.

Regression coverage includes direct/transitive feedback, groups, bidirectional encoder/knob sweeps, compressor initial focus and navigation, and serial EQ audio/second-slot spectrum PCM. The user rechecked two EQs on Track A and confirmed correct behaviour; chain DSP was not changed.

After installing, safely eject in Windows, return with B3 on MPC and restart the device.

# r26f candidate — fast knob movements

[Русский](RELEASE_NOTES-r26f.ru.md) · Addresses [issue #2](https://github.com/AlekseyMalykh/am-mod/issues/2).

Fast K1/K2/K3 sweeps could leave a parameter at an intermediate value after the physical knob reached its end stop. The reproduced queue failure let the newest value bypass a full queue, after which older movements overwrote it. The update retains the latest absolute knob positions in arrival order, keeping the final focus on the last knob moved.

Buttons, pads and relative encoder movements keep their order; knob movements are not merged across a Shift/action or ownership change. Physical Main Volume handling is unchanged. This candidate also includes [experimental FAT32 support from r26e](RELEASE_NOTES-r26e.md); exFAT remains supported. Firmware: **1.3.0.12**.

Validation: production controller/FIFO code on ARM/QEMU reproduced the old failure (requested 0, retained 28). The fix passes minimum/maximum bursts on K1/K2/K3, ring-buffer wrap/full tests, 50,000 concurrent updates, naming and resampling route checks, with and without UBSan. The full native preload constructor installs all 50 callbacks successfully. Native parameter/audio receivers in the input tests are recording fixtures.

**Physical Warp verification is pending.** Test fast sweeps to both ends on Warp and other parameter pages, including Shift functions and a change of selected pad. The actual Warp screen and hardware ADC were not exercised by these tests. This candidate has not been published and does not close the issue on its own.

Reproduce: from `sd-mod-session/`, run `python3 verify_knob_events.py`, `python3 build_chords_candidate.py`, then `python3 package_knob_fix.py ../am-mod-r26e.zip` with the ARM toolchain/sysroot available. Install the complete `autorun/` folder from the candidate, or use installer 1.0.2; projects and samples do not need to be moved or reformatted.

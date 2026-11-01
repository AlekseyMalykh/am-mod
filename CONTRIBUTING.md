# Contributing

Issues and pull requests are welcome, in English or Russian.

Use the [bug/feature forms and Discussions](docs/FEEDBACK.md). Check the
[known-issues list](docs/KNOWN_ISSUES.md) first; maintainers should update its
status when a report is reproduced, fixed or confirmed on hardware.

- **Never commit firmware**, factory content, files extracted from either, release zips,
  SD-card contents or device identifiers (serial numbers, volume IDs, keys). The
  whitelist `.gitignore` exists for that reason: add new top-level folders to it on
  purpose, not by `git add -f`.
- **Open an issue first** for a large change, so we can agree on the approach.
- **Test where you can and say where you tested**: ARM/QEMU checks (`verify_*.py`),
  the [Virtual MPC Sample](https://github.com/orrpunk-art/virtual-mpc-sample) emulator,
  or a real MPC Sample (firmware 1.3.0.12). A passing emulator run does not replace a
  hardware check; docs here keep the two apart.
- **Keep the stock system stock**: the mod patches only its RAM copy of the app and
  never writes the firmware, boot loader or internal storage outside its session.
- Run the installer tests before touching `installer/`:
  `python -m unittest discover -s installer`.

AI-assisted contributions are fine. You are responsible for what you submit: read it,
run it, and say in the pull request what you tested.

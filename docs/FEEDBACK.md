# Feedback, bugs and ideas

[Русский](FEEDBACK.ru.md)

## Installer 1.1.0 news and bug reports

**GitHub** opens the repository and **All releases** opens release history. **Release news** shows the latest stable release description, including features and fixes. **Refresh news** reloads it and **Download this release** downloads that displayed release. Selecting a local ZIP shows its bundled README offline.

To report a problem, connect the MPC SD card to the computer, select it in the main window and choose **Report a bug**. Enter the installed mod version, reproduction steps, expected behaviour and actual behaviour. Review the listed journals, then choose **Save diagnostic ZIP**. The archive includes the report, installer log, installed-version metadata and available `autorun/MPC_MOD_EVENTS.txt`, `autorun/MPC_CLONE_TRIAL.txt` and `autorun/MPC_CLONE_ALREADY_ATTEMPTED.txt`. Log collection can be disabled.

Review the ZIP for project names and file paths before sharing. **Open GitHub issue** opens a draft. Attach the ZIP in the browser and submit it. For long descriptions, use **Copy report** to paste the complete report into the issue. Submitting requires a GitHub account.

- **Something is broken:** [report a bug](https://github.com/AlekseyMalykh/am-mod/issues/new?template=bug_report.yml).
- **A concrete feature request:** [suggest a feature](https://github.com/AlekseyMalykh/am-mod/issues/new?template=feature_request.yml).
- **Early ideas:** [Ideas discussions](https://github.com/AlekseyMalykh/am-mod/discussions/categories/ideas).
- **Questions and installation help:** [Q&A](https://github.com/AlekseyMalykh/am-mod/discussions/categories/q-a).
- **Testing results and conversation:** [General](https://github.com/AlekseyMalykh/am-mod/discussions/categories/general).
- **Before reporting:** [known issues and fixed versions](KNOWN_ISSUES.md), then check existing issues.

English and Russian are welcome. Please include the exact mod version or ZIP
name, stock firmware version, SD filesystem, screen and steps. For knobs,
include the takeover mode; for projects, include Internal/External storage and
the startup/normal-power-off sequence (Mod Off only on older releases). One bug per issue makes the status easier to track.
Screenshots and short videos help; do not upload firmware or private project content.

An existing report is the best place for another reproduction or test result.
New ideas are welcome, but an entry in the list is not a promise of a release date.
Bug reports, implemented fixes and verified hardware results are recorded separately.

Maintainers should update the known-issues list when a report is reproduced,
fixed or confirmed on hardware. The list is maintained manually; Discussions
does not automatically synchronise it with releases or issue status.

For pad colours, include **Project → Mod Settings → Pads: Standard / Am MOD**, bank and selection state. For Note Repeat, specify held/latched mode and release timing.
[Current r26x readiness audit](RELEASE_READINESS-r26x.md): a code fix does not close the original MPC scenario by itself.

# MPC Sample Mod Installer 1.1.0

Installer update for Am MOD, 12 November 2026. Compatible with the r28d mod package and older supported packages.

## What's new

- GitHub and release-history buttons in the main window.
- A Release news tab with the latest stable release description, new features and bug fixes. Refresh news reloads it, and Download this release downloads the displayed package.
- Offline release notes from the README inside a selected local ZIP.
- A bug report form with the installed mod version, reproduction steps, expected behaviour and actual behaviour.
- A diagnostic ZIP containing the report, installer log and available SD journals. The form lists the files before saving, and log collection can be disabled.
- Copy report copies the full description. Open GitHub issue opens a prefilled draft for review and submission.

## Install or update the mod

1. Download and run MPC-Sample-Mod-Installer.exe.
2. Connect the MPC SD card to your computer and select it in the installer.
3. Choose Latest release, or open a downloaded am-mod ZIP. You can also download the release displayed in Release news.
4. Click Install and confirm the selected version and card.
5. Safely eject the card, insert it into the MPC and restart.

The installer targets MPC Sample firmware 1.3.0.12. Back up projects and samples before updating.

## Send a bug report with journals

1. Reproduce the problem on the MPC, then connect its SD card to your computer.
2. Select the card in the installer and click Report a bug.
3. Check the installed mod version. Enter a summary, reproduction steps, expected result and actual result.
4. Review the listed files. Clear Include available SD journals and installer log if you want a description-only report.
5. Click Save diagnostic ZIP and choose a location on your computer.
6. Review the archive. Logs may contain project names and file paths.
7. Click Open GitHub issue. Sign in to GitHub if necessary.
8. Attach the saved ZIP in the browser, review the draft and submit it. For a long description, use Copy report and paste the complete text into the issue.

The archive can include MPC_MOD_VERSION.json and autorun/MPC_MOD_EVENTS.txt, autorun/MPC_CLONE_TRIAL.txt and autorun/MPC_CLONE_ALREADY_ATTEMPTED.txt. Missing journals are noted in collection.txt. Large journals retain their last 2 MiB. Report preparation also works without a selected SD card.

Journal attachments and final submission are performed in the browser. A GitHub account is required to submit an issue.

## Verification

Installer tests passed on Windows and Linux. The Windows executable was built with PyInstaller 6.11.1. Live GitHub release metadata and offline r28d notes were checked. Visual inspection remains incomplete because the window-capture tool timed out.

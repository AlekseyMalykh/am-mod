# Installing and updating Am MOD r30d-beta1

[Home](../README.md) · [User guide](USER_GUIDE.md) · [Русский](INSTALL.ru.md) · [Español](INSTALL.es.md)

## Before installation

Use an **Akai MPC Sample**, firmware **1.3.0.12** (the device displays 1.3.0). Other MPC models/firmware are unsupported. Back up the entire card, especially `MPC-Sample`, projects, samples and settings. Prefer exFAT. FAT32 is experimental and cannot hold files of 4 GiB or larger. Give the card an ASCII volume label without spaces, such as `MPC`.

The mod runs an application copy in RAM without flashing firmware. In the r30d-beta1 storage candidate, **Internal user projects, samples and audio exports persist on flash**; system data outside those folders remains isolated. K3 selects Internal/External in native Save/Export dialogs. Older SD-only packages retain their original limits. Stock firmware or older mods may drop newer project fields when resaving; keep a separate project copy.

![Three supported installation layouts](media/r30c/install-en.png)

## Recommended: installer 1.4.0

1. Download `am-mod-r30d-beta1.zip`, `MPC-Sample-Mod-Installer.exe` and `SHA256SUMS.txt` from the [r30d-beta1 release](https://github.com/AlekseyMalykh/am-mod/releases/tag/r30d-beta1). Keep the mod ZIP intact for the installer.
2. On the MPC save your current project to External. Connect the card using a reader or Project → SD Card Access. For first installation, use the stock USB-drive screen or a reader.
3. Run the installer, choose the mod ZIP, refresh the card list and select the correct card by drive, label and size. Press Install.
4. The installer validates the package, detects the layout below, backs up the existing autorun to `~/AM-Mod-Backups/`, copies the files and verifies hashes. This is an autorun backup, not a project backup. Do not unplug during copying.
5. Safely eject the drive in the computer OS. From AM Mod's SD screen use B3 Disconnect, then B3 Return; in stock follow its disconnect prompt.
6. Power off and on. A copied update does not replace the app already running in RAM. In a boot menu choose AM Mod. Check **Project → Mod Settings → About** for r30d-beta1.

| Existing card | Installer destination | What stays |
|---|---|---|
| Plain card | `autorun/` | Projects/samples outside autorun |
| mpc-boot-thingy | `autorun/apps/am-mod/autorun/`, with `boot.entry` in its parent | Boot menu and other apps |
| D.S.B Boot | `autorun/os/AM-Mod/` | Boot menu and other OS folders |

D.S.B needs the updated AM package's `sampleboot.inplace` and compatible launcher. Leave the marker in place. An old incompatible AM package or incomplete D.S.B layout is rejected before writing. If both supported boot-loader markers are present, the installer asks you to resolve the ambiguity rather than guessing.

The installer supports an **already installed** boot menu; it does not download or install D.S.B, mpc-boot-thingy or SampQOL for you. Install those from their authors separately. No SampQOL files are redistributed in the AM ZIP. From AM Mod, *Mod Settings → About → Exit to bootloader* returns to the D.S.B menu (see [BOOTLOADERS.md](BOOTLOADERS.md)); with other loaders it starts stock firmware.

## macOS / Linux and command line

Install Python 3 and use `installer/mpc_mod_installer.py` from the source repository. The same automatic layout detection applies.

```bash
python3 installer/mpc_mod_installer.py cards
python3 installer/mpc_mod_installer.py install am-mod-r30d-beta1.zip --card /Volumes/MPC --dry-run
python3 installer/mpc_mod_installer.py install am-mod-r30d-beta1.zip --card /Volumes/MPC
python3 installer/mpc_mod_installer.py status --card /Volumes/MPC
```

On Windows replace the mount point with `E:` (use your actual card drive); on Linux use the mounted card path. A dry run writes nothing. `status` checks the installed managed files. The GUI needs Tk; the CLI does not.

## Manual installation

With the MPC off or the card safely shared to the computer, back up the old files. On a plain card replace the **complete** `autorun/` folder with the one from the ZIP. Avoid an extra `autorun/autorun/` nesting. Do not merge only the library from one version into another launcher.

On **D.S.B**, copy the **contents** of the ZIP's autorun folder into `autorun/os/AM-Mod/`, keeping `sampleboot.inplace`. On **mpc-boot-thingy**, prefer the installer: it creates the nested autorun and boot.entry together. Never replace the boot menu's root autorun with AM's.

## Updating, removing and recovery

- Update with the same installer procedure; it detects an existing boot menu again. Restart after safely ejecting.
- To remove AM installed by this tool: `python3 installer/mpc_mod_installer.py remove --card /Volumes/MPC`. In boot-menu mode it removes AM's managed entry, preserving the loader and other systems. Backups are kept.
- For emergency stock startup, power off and remove the mod card. To use the card with stock, disable/rename its autorun while it is connected to the computer. On a boot-menu card prefer that loader's Stock option.
- To restore, power off and restore the complete matching autorun backup. Do not combine different backup versions. Restore your independent project backup if needed.
- If the package will not boot: check firmware, card label, filesystem, folder nesting and complete extraction; report the release, steps, active bootloader and logs through the installer's diagnostic report. Never send private projects unless you intend to share them.

## First-session check

Start with a copy of a small project. Play a pad; open Mixer; edit/bypass an effect. Save to Internal and External using K3, then power-cycle and reload. Check a saved sample and an audio export too. Try MIDI, imports and Sample Swap separately. The final package still needs physical-device checks, including third-party bootloaders.

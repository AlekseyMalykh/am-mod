#!/usr/bin/env python3
"""AM Mod installer (SD card mod for the Akai MPC Sample).

Copies a release package (am-mod-*.zip) onto the SD card you use with
the Akai MPC Sample, so the device offers to start the mod when the card is
inserted. It only ever touches two things on the card:

  * the ``autorun/`` folder (the SD autorun package the MPC looks for);
  * ``MPC_MOD_VERSION.json`` (what was installed, with file hashes).

It never formats, partitions or writes to a raw disk, and never touches your
projects and samples (``MPC-Sample/``). Whatever was in ``autorun/`` before
is copied to a backup folder on this computer first.

Usage:
    python mpc_mod_installer.py                 # window (Tk)
    python mpc_mod_installer.py cards           # list removable cards
    python mpc_mod_installer.py install PACKAGE.zip --card E:
    python mpc_mod_installer.py install --latest --card /Volumes/MPC
    python mpc_mod_installer.py remove --card E:
    python mpc_mod_installer.py status --card E:

No dependencies beyond the Python 3.8+ standard library.
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import io
import json
import os
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

__version__ = "1.0.3"
REPO = "AlekseyMalykh/am-mod"
VERSION_FILE = "MPC_MOD_VERSION.json"
AUTORUN = "autorun"
# Files that make the MPC run the package. They are written last, so an
# interrupted copy never leaves a card that starts a half-written mod.
ENTRY_ORDER = ("mod-launcher.sh", "autorun.sh", "autorun.json", "autorun.check")
REQUIRED = {"autorun.json", "autorun.sh", "autorun.check", "mod-launcher.sh"}
# Constant the stock firmware 1.3.0.12 appends when it checks autorun.check.
# Used here only to detect a damaged or incomplete download.
AUTORUN_CHECK_SALT = b"f7bed1c9-7e90-4731-9194-c996666b4331"
MAX_PACKAGE = 256 << 20


class InstallError(Exception):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


# --------------------------------------------------------------------------
# Package


class Package:
    """A release zip: autorun/* payload plus optional docs."""

    def __init__(self, data: bytes, name: str):
        if len(data) > MAX_PACKAGE:
            raise InstallError("Package is larger than 256 MB; this is not a mod release.")
        self.name = name
        self.sha256 = sha256(data)
        try:
            zf = zipfile.ZipFile(io.BytesIO(data))
        except zipfile.BadZipFile as e:
            raise InstallError(f"{name} is not a zip file ({e}).")
        bad = zf.testzip()
        if bad:
            raise InstallError(f"{name} is damaged (first bad entry: {bad}).")
        self.files: dict[str, bytes] = {}
        for info in zf.infolist():
            if info.is_dir():
                continue
            p = PurePosixPath(info.filename)
            if p.is_absolute() or ".." in p.parts or "\\" in info.filename:
                raise InstallError(f"Unsafe path in package: {info.filename}")
            if len(p.parts) == 2 and p.parts[0] == AUTORUN:
                self.files[p.parts[1]] = zf.read(info)
            elif p.parts[0] == AUTORUN:
                raise InstallError(f"Unexpected nested file in autorun/: {info.filename}")
        missing = REQUIRED - set(self.files)
        if missing:
            raise InstallError(f"{name} is not an MPC Sample mod package: missing autorun/{', autorun/'.join(sorted(missing))}.")
        expected = sha256(self.files["autorun.json"] + self.files["autorun.sh"] + AUTORUN_CHECK_SALT)
        if self.files["autorun.check"].strip().decode("ascii", "replace") != expected:
            raise InstallError("autorun.check does not match autorun.json/autorun.sh: the package is damaged or not for firmware 1.3.0.12.")
        try:
            manifest = json.loads(self.files["autorun.json"])
            self.message = manifest["value0"]["scripts"][0].get("message", "")
        except (ValueError, KeyError, IndexError, TypeError):
            raise InstallError("autorun.json is not a valid MPC autorun manifest.")
        self.revision = self._revision()
        # Older packages have no capability declaration and require exFAT.
        self.filesystems = {"exfat"}
        for line in self.files["autorun.sh"].decode("utf-8", "replace").splitlines():
            if line.startswith("# sd-filesystems="):
                self.filesystems = set(line.split("=", 1)[1].split(","))
                if not self.filesystems or not self.filesystems <= {"exfat", "vfat"}:
                    raise InstallError("Invalid SD filesystem declaration in autorun.sh.")

    def _revision(self) -> str:
        for line in self.files["autorun.sh"].decode("utf-8", "replace").splitlines():
            if "revision=" in line:
                tail = line.split("revision=", 1)[1]
                return tail.split()[0].strip("'\";")
        return Path(self.name).stem

    @classmethod
    def from_file(cls, path: Path) -> "Package":
        return cls(Path(path).read_bytes(), Path(path).name)

    def ordered(self) -> list[str]:
        rank = {n: i for i, n in enumerate(ENTRY_ORDER)}
        return sorted(self.files, key=lambda n: (rank.get(n, -1), n))


def latest_release(log=print) -> Package:
    """Download the newest am-mod-*.zip asset from GitHub Releases."""
    url = f"https://api.github.com/repos/{REPO}/releases/latest"
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json",
                                               "User-Agent": "am-mod-installer"})
    with urllib.request.urlopen(req, timeout=30) as r:
        rel = json.load(r)
    assets = [a for a in rel.get("assets", []) if a["name"].startswith(("am-mod-", "mpc-sample-")) and a["name"].endswith(".zip")]
    if not assets:
        raise InstallError(f"Release {rel.get('tag_name')} has no am-mod-*.zip asset.")
    a = assets[0]
    log(f"Downloading {a['name']} ({a['size'] // 1024} KB) from release {rel.get('tag_name')}...")
    req = urllib.request.Request(a["browser_download_url"], headers={"User-Agent": "am-mod-installer"})
    with urllib.request.urlopen(req, timeout=120) as r:
        data = r.read(MAX_PACKAGE + 1)
    return Package(data, a["name"])


# --------------------------------------------------------------------------
# Cards


class Card:
    def __init__(self, root: Path, label: str = "", fs: str = "", size: int = 0):
        self.root = Path(root)
        self.label = label
        self.fs = fs
        self.size = size

    def __str__(self) -> str:
        gb = f"{self.size / 1e9:.1f} GB" if self.size else "?"
        parts = [str(self.root), self.label or "(no label)", self.fs or "?", gb]
        if (self.root / "MPC-Sample").is_dir():
            parts.append("MPC-Sample")
        if (self.root / VERSION_FILE).is_file():
            parts.append("mod: " + installed_revision(self.root))
        return "  ".join(parts)


def installed_revision(root: Path) -> str:
    try:
        return json.loads((Path(root) / VERSION_FILE).read_text(encoding="utf-8-sig")).get("revision", "?")
    except (OSError, ValueError):
        return "?"


def _windows_card(root: Path) -> Card | None:
    import ctypes
    from ctypes import wintypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    label = ctypes.create_unicode_buffer(261)
    fs = ctypes.create_unicode_buffer(261)
    serial, maxlen, flags = wintypes.DWORD(), wintypes.DWORD(), wintypes.DWORD()
    if not k.GetVolumeInformationW(str(root), label, 261, ctypes.byref(serial), ctypes.byref(maxlen),
                                   ctypes.byref(flags), fs, 261):
        return None
    try:
        size = shutil.disk_usage(root).total
    except OSError:
        size = 0
    return Card(root, label.value, fs.value, size)


def _windows_cards() -> list[Card]:
    import ctypes
    k = ctypes.WinDLL("kernel32", use_last_error=True)
    cards = []
    mask = k.GetLogicalDrives()
    for i in range(26):
        if not mask & (1 << i):
            continue
        drive = f"{chr(65 + i)}:\\"
        if k.GetDriveTypeW(drive) != 2:  # DRIVE_REMOVABLE
            continue
        card = _windows_card(Path(drive))
        if card:
            cards.append(card)
    return cards


def _mac_card(root: Path) -> Card:
    try:
        result = subprocess.run(["diskutil", "info", "-plist", str(root)],
                                capture_output=True, check=True, timeout=10)
        info = plistlib.loads(result.stdout)
        if not isinstance(info, dict) or Path(info.get("MountPoint", "")) != root:
            return Card(root)
        return Card(root, info.get("VolumeName", ""), info.get("FilesystemType", ""),
                    info.get("TotalSize", 0))
    except (OSError, subprocess.SubprocessError, plistlib.InvalidFileException, ValueError):
        # Keep the card visible, but check_card must reject unverified metadata.
        return Card(root)


def _linux_label(device: str) -> str:
    try:
        result = subprocess.run(["lsblk", "--nodeps", "--noheadings", "--output", "LABEL", device],
                                capture_output=True, text=True, check=True, timeout=10)
        return result.stdout.rstrip("\r\n")
    except (OSError, subprocess.SubprocessError):
        return ""


def _posix_cards() -> list[Card]:
    cards = []
    if sys.platform == "darwin":
        base = Path("/Volumes")
        for p in sorted(base.iterdir()) if base.exists() else []:
            try:
                if p.is_dir() and not p.is_symlink() and os.path.ismount(p):
                    cards.append(_mac_card(p))
            except OSError:
                pass
        return cards
    try:
        mounts = Path("/proc/mounts").read_text().splitlines()
    except OSError:
        return cards
    for line in mounts:
        dev, mnt, fs, *_ = line.split()
        mnt = re.sub(r"\\([0-7]{3})", lambda m: chr(int(m[1], 8)), mnt)
        if fs.lower() not in ("vfat", "exfat", "fuseblk", "msdos"):
            continue
        if not mnt.startswith(("/media/", "/run/media/", "/mnt/")):
            continue
        try:
            st = os.statvfs(mnt)
        except OSError:
            continue
        cards.append(Card(Path(mnt), _linux_label(dev), fs, st.f_blocks * st.f_frsize))
    return cards


def list_cards() -> list[Card]:
    return _windows_cards() if os.name == "nt" else _posix_cards()


def card_from_arg(arg: str) -> Card:
    p = Path(arg if not (len(arg) == 2 and arg[1] == ":") else arg + "\\")
    for c in list_cards():
        try:
            if c.root.resolve() == p.resolve():
                return c
        except OSError:
            pass
    if not p.is_dir():
        raise InstallError(f"{arg} is not a mounted card.")
    # Some Windows readers report as fixed. Read their actual volume metadata too.
    if os.name == "nt" and p.resolve() == Path(p.anchor):
        card = _windows_card(p)
        if card:
            return card
    raise InstallError(f"Cannot verify the filesystem and volume name of {p}. "
                       "Choose the SD card root shown by the cards command or Refresh; "
                       "if needed, connect the card through a card reader.")


def check_card(card: Card, log=print) -> None:
    root = card.root
    if not root.is_dir():
        raise InstallError(f"{root} is not available.")
    resolved = root.resolve()
    home = Path.home().resolve()
    system = Path(os.environ.get("SystemDrive", "C:") + "\\").resolve() if os.name == "nt" else Path("/")
    def inside(child: Path, parent: Path) -> bool:
        try:
            child.relative_to(parent)
            return True
        except ValueError:
            return False
    if resolved == system or inside(home, resolved) or inside(Path(sys.executable).resolve(), resolved):
        raise InstallError(f"{root} looks like a system disk, not an SD card.")
    if not card.fs:
        raise InstallError(f"Cannot verify the filesystem of {root}. Installation stopped before writing files. "
                           "AM Mod requires an exFAT or FAT32 card.")
    if card.fs.lower() not in ("exfat", "fat32", "vfat", "msdos"):
        raise InstallError(f"{root} uses {card.fs}. AM Mod supports exFAT and FAT32/MS-DOS (FAT32). "
                           "Back up your projects and samples before changing the filesystem.")
    if not re.fullmatch(r"[A-Za-z0-9._-]+", card.label):
        raise InstallError("The current AM Mod requires a verified SD volume name containing only "
                           "A-Z, a-z, 0-9, dot, underscore or hyphen (for example MPC). "
                           "Names with spaces or Cyrillic letters are not supported. "
                           "Set the card's volume name and press Refresh; formatting is not needed to rename it.")
    if card.size and card.size > 2.2e12:
        raise InstallError(f"{root} is {card.size / 1e12:.1f} TB; that is not an SD card.")
    a = root / AUTORUN
    if a.is_symlink() or (a.exists() and not a.is_dir()):
        raise InstallError(f"{a} exists but is not a folder.")
    if not (root / "MPC-Sample").is_dir():
        log("Note: no MPC-Sample folder on this card yet. That is fine for a new card; "
            "the MPC creates it the first time it saves.")


# --------------------------------------------------------------------------
# Install / remove


def backup_dir() -> Path:
    base = Path(os.environ.get("MPC_MOD_BACKUP_DIR") or (Path.home() / "AM-Mod-Backups"))
    stamp = _dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    for i in range(100):
        d = base / (stamp if i == 0 else f"{stamp}-{i}")
        try:
            d.mkdir(parents=True, exist_ok=False)
            return d
        except FileExistsError:
            continue
    raise InstallError(f"Cannot create a backup folder in {base}.")


def backup_card(root: Path, log=print) -> Path | None:
    a = root / AUTORUN
    v = root / VERSION_FILE
    if not a.exists() and not v.exists():
        return None
    d = backup_dir()
    if a.exists():
        shutil.copytree(a, d / AUTORUN)
        for p in a.rglob("*"):
            if p.is_file():
                q = d / AUTORUN / p.relative_to(a)
                if file_sha256(p) != file_sha256(q):
                    raise InstallError(f"Backup of {p} does not match; nothing was changed on the card.")
    if v.exists():
        shutil.copy2(v, d / VERSION_FILE)
    log(f"Backed up the previous autorun/ to {d}")
    return d


def _write(path: Path, data: bytes) -> None:
    tmp = path.with_name("." + path.name + ".mpcmod-tmp")
    with open(tmp, "wb") as f:
        f.write(data)
        f.flush()
        os.fsync(f.fileno())
    if path.exists():
        os.chmod(path, 0o666)
    os.replace(tmp, path)


def _sync(root: Path) -> None:
    if hasattr(os, "sync"):
        os.sync()


def install(pkg: Package, card: Card, log=print, dry_run: bool = False) -> dict:
    check_card(card, log)
    filesystem = "exfat" if card.fs.lower() == "exfat" else "vfat"
    if filesystem not in pkg.filesystems:
        raise InstallError(f"Package {pkg.revision} does not support {card.fs}. "
                           "FAT32 requires AM Mod r26e or later; select a compatible package. Nothing was written.")
    root = card.root
    log(f"Package: {pkg.name}  revision {pkg.revision}  ({len(pkg.files)} files, sha256 {pkg.sha256[:12]}...)")
    need = sum(len(b) for b in pkg.files.values())
    free = shutil.disk_usage(root).free
    if free < need + (8 << 20):
        raise InstallError(f"Not enough space on {root}: need {need >> 20} MB, have {free >> 20} MB.")
    a = root / AUTORUN
    existing = sorted(p.name for p in a.iterdir()) if a.is_dir() else []
    foreign = [n for n in existing if n not in pkg.files] if existing and not (root / VERSION_FILE).exists() else []
    if foreign:
        log(f"Warning: autorun/ holds files from another package ({', '.join(foreign[:5])}...). "
            "They will be moved to the backup.")
    if dry_run:
        log("Dry run: nothing written.")
        return {}
    backup = backup_card(root, log)
    a.mkdir(exist_ok=True)
    # Remove files that are not part of the new package (they are in the backup).
    for n in existing:
        if n not in pkg.files:
            p = a / n
            if p.is_file() and not p.is_symlink():
                os.chmod(p, 0o666)
                p.unlink()
    # The launcher files go last so a half-finished copy is never started.
    for n in pkg.ordered():
        _write(a / n, pkg.files[n])
        log(f"  wrote autorun/{n}")
    info = {
        "revision": pkg.revision,
        "package": pkg.name,
        "package_sha256": pkg.sha256,
        "payload_sha256": {n: sha256(b) for n, b in sorted(pkg.files.items())},
        "installed_at": _dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "installer": f"mpc_mod_installer {__version__} ({platform.system()})",
        "backup": str(backup) if backup else None,
    }
    _write(root / VERSION_FILE, (json.dumps(info, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    _sync(root)
    log("Verifying...")
    for n, b in pkg.files.items():
        if file_sha256(a / n) != sha256(b):
            raise InstallError(f"autorun/{n} on the card does not match the package. Re-insert the card and try again.")
    log(f"Done: {pkg.revision} is on {root}. Eject the card safely before removing it.")
    return info


def remove(card: Card, log=print) -> None:
    root = card.root
    v = root / VERSION_FILE
    if not v.exists():
        raise InstallError(f"No {VERSION_FILE} on {root}: no mod installed by this tool.")
    info = json.loads(v.read_text(encoding="utf-8-sig"))
    backup_card(root, log)
    a = root / AUTORUN
    for n in info.get("payload_sha256", {}):
        p = a / PurePosixPath(n).name
        if p.is_file() and not p.is_symlink():
            os.chmod(p, 0o666)
            p.unlink()
    if a.is_dir() and not any(a.iterdir()):
        a.rmdir()
    os.chmod(v, 0o666)
    v.unlink()
    _sync(root)
    log(f"Removed {info.get('revision', 'the mod')} from {root}. Projects and samples were not touched.")


def status(card: Card, log=print) -> None:
    v = card.root / VERSION_FILE
    if not v.exists():
        log(f"{card.root}: no mod installed by this tool.")
        return
    info = json.loads(v.read_text(encoding="utf-8-sig"))
    bad = [n for n, h in info.get("payload_sha256", {}).items()
           if not (card.root / AUTORUN / n).is_file() or file_sha256(card.root / AUTORUN / n) != h]
    log(f"{card.root}: {info.get('revision')} installed {info.get('installed_at')}; "
        + ("all files intact." if not bad else f"{len(bad)} files changed or missing: {', '.join(bad[:5])}"))


# --------------------------------------------------------------------------
# GUI


def gui() -> None:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    win = tk.Tk()
    win.title(f"AM Mod installer {__version__}")
    win.minsize(640, 420)
    state = {"pkg": None, "cards": []}
    frm = ttk.Frame(win, padding=12)
    frm.pack(fill="both", expand=True)
    frm.columnconfigure(1, weight=1)

    ttk.Label(frm, text="1. Mod package (.zip):").grid(row=0, column=0, sticky="w")
    pkg_var = tk.StringVar(value="not selected")
    ttk.Label(frm, textvariable=pkg_var).grid(row=0, column=1, sticky="w", padx=6)
    bar = ttk.Frame(frm)
    bar.grid(row=0, column=2, sticky="e")

    ttk.Label(frm, text="2. SD card:").grid(row=1, column=0, sticky="w", pady=(10, 0))
    card_var = tk.StringVar()
    combo = ttk.Combobox(frm, textvariable=card_var, state="readonly")
    combo.grid(row=1, column=1, sticky="ew", padx=6, pady=(10, 0))

    out = tk.Text(frm, height=14, wrap="word")
    out.grid(row=3, column=0, columnspan=3, sticky="nsew", pady=10)
    frm.rowconfigure(3, weight=1)

    def log(msg: str) -> None:
        win.after(0, lambda: (out.insert("end", msg + "\n"), out.see("end")))

    def refresh() -> None:
        state["cards"] = list_cards()
        combo["values"] = [str(c) for c in state["cards"]]
        if state["cards"]:
            combo.current(0)
        else:
            card_var.set("")
            log("No removable card found. Insert the SD card and press Refresh.")

    def set_pkg(p: Package) -> None:
        state["pkg"] = p
        pkg_var.set(f"{p.name}  ({p.revision})")
        log(f"Package OK: {p.name}, revision {p.revision}.")

    def pick() -> None:
        f = filedialog.askopenfilename(title="Mod package", filetypes=[("Zip", "*.zip")])
        if f:
            try:
                set_pkg(Package.from_file(Path(f)))
            except InstallError as e:
                messagebox.showerror("Package", str(e))

    def run(fn) -> None:
        def worker():
            try:
                fn()
            except (InstallError, OSError) as e:
                error = str(e)
                log(f"ERROR: {error}")
                win.after(0, lambda message=error: messagebox.showerror("AM Mod", message))
        threading.Thread(target=worker, daemon=True).start()

    def latest() -> None:
        run(lambda: (lambda p: win.after(0, lambda: set_pkg(p)))(latest_release(log)))

    def selected() -> Card | None:
        i = combo.current()
        return state["cards"][i] if 0 <= i < len(state["cards"]) else None

    def do_install() -> None:
        c, p = selected(), state["pkg"]
        if not p or not c:
            messagebox.showwarning("AM Mod", "Choose a package and a card first.")
            return
        if messagebox.askyesno("Install", f"Install {p.revision} on {c.root}?\n\n"
                               "Only autorun/ and MPC_MOD_VERSION.json are written; the old autorun/ is backed up "
                               "on this computer. Projects and samples are not touched."):
            run(lambda: install(p, c, log))

    def do_remove() -> None:
        c = selected()
        if c and messagebox.askyesno("Remove", f"Remove the mod from {c.root}?"):
            run(lambda: remove(c, log))

    ttk.Button(bar, text="Open .zip...", command=pick).pack(side="left")
    ttk.Button(bar, text="Latest release", command=latest).pack(side="left", padx=(6, 0))
    ttk.Button(frm, text="Refresh", command=refresh).grid(row=1, column=2, sticky="e", pady=(10, 0))
    btns = ttk.Frame(frm)
    btns.grid(row=4, column=0, columnspan=3, sticky="e")
    ttk.Button(btns, text="Remove mod", command=do_remove).pack(side="left")
    ttk.Button(btns, text="Install", command=do_install).pack(side="left", padx=(6, 0))

    log("Firmware 1.3.0.12 only. The mod runs a patched copy of the stock app in RAM; it does not flash the MPC.")
    log("SD: exFAT or experimental FAT32/MS-DOS (FAT32). FAT32 requires package r26e or later. Volume name: MPC, for example; no spaces or Cyrillic.")
    log("Start: insert the card and power on, or insert it after the normal screen appears. The mod starts automatically.")
    log("Exit: save your project, then use the MPC power button. Older packages may also offer Project > Mod Off.")
    refresh()
    win.mainloop()


# --------------------------------------------------------------------------
# CLI


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Install AM Mod onto the SD card of an Akai MPC Sample.")
    ap.add_argument("--version", action="version", version=__version__)
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("cards", help="list removable cards")
    i = sub.add_parser("install", help="install a package")
    i.add_argument("package", nargs="?", help="am-mod-*.zip")
    i.add_argument("--latest", action="store_true", help="download the latest GitHub release")
    i.add_argument("--card", required=True, help="drive letter (E:) or mount point")
    i.add_argument("--dry-run", action="store_true")
    i.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    r = sub.add_parser("remove", help="remove the mod from a card")
    r.add_argument("--card", required=True)
    s = sub.add_parser("status", help="show what is installed")
    s.add_argument("--card", required=True)
    args = ap.parse_args(argv)
    try:
        if args.cmd is None:
            gui()
        elif args.cmd == "cards":
            cards = list_cards()
            print("\n".join(map(str, cards)) or "No removable cards found.")
        elif args.cmd == "install":
            if bool(args.package) == bool(args.latest):
                ap.error("give a package file or --latest")
            pkg = latest_release() if args.latest else Package.from_file(Path(args.package))
            card = card_from_arg(args.card)
            if not args.yes and not args.dry_run:
                if input(f"Install {pkg.revision} on {card.root}? [y/N] ").strip().lower() not in ("y", "yes", "д", "да"):
                    return 1
            install(pkg, card, dry_run=args.dry_run)
        elif args.cmd == "remove":
            remove(card_from_arg(args.card))
        elif args.cmd == "status":
            status(card_from_arg(args.card))
    except InstallError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())

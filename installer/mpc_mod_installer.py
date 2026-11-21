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
import urllib.parse
import urllib.request
import webbrowser
import zipfile
from pathlib import Path, PurePosixPath

__version__ = "1.4.0"
REPO = "AlekseyMalykh/am-mod"
GITHUB_URL = f"https://github.com/{REPO}"
VERSION_FILE = "MPC_MOD_VERSION.json"
AUTORUN = "autorun"
# Files that make the MPC run the package. They are written last, so an
# interrupted copy never leaves a card that starts a half-written mod.
# mpc-boot-thingy (github.com/garrettjwilke/mpc-boot-thingy) owns autorun/ when
# this marker exists. The mod is then installed as one of its apps instead.
BOOT_MARK = "autorun/system/mpc-boot-thingy.mark"
APP_DIR = "autorun/apps/am-mod"
DSB_DIR = "autorun/os/AM-Mod"
APP_ENTRY = "title=AM Mod\nrun=autorun/autorun.sh\ntakeover=1\n"
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
        self.notes_en = ""
        self.notes_ru = ""
        for info in zf.infolist():
            if info.is_dir():
                continue
            p = PurePosixPath(info.filename)
            if p.is_absolute() or ".." in p.parts or "\\" in info.filename:
                raise InstallError(f"Unsafe path in package: {info.filename}")
            if len(p.parts) == 2 and p.parts[0] == AUTORUN:
                self.files[p.parts[1]] = zf.read(info)
            elif info.filename in ("README.ru.md", "README.md"):
                if info.file_size <= 262144:
                    text = zf.read(info).decode("utf-8", "replace")
                    if info.filename == "README.ru.md":
                        self.notes_ru = text
                    else:
                        self.notes_en = text
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

    @property
    def notes(self) -> str:
        return self.notes_for("ru")

    def notes_for(self, lang: str) -> str:
        """Notes in the requested language; falls back to the other one."""
        first, second = (self.notes_ru, self.notes_en) if lang == "ru" else (self.notes_en, self.notes_ru)
        return first or second

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


def split_release_body(body: str) -> dict[str, str]:
    """Split a GitHub release body into languages.

    Sections start with a line `<!-- lang:en -->` or `<!-- lang:ru -->`.
    Text before the first marker is shared. Without markers the whole body
    is shown for both languages."""
    sections: dict[str, list[str]] = {"": []}
    current = ""
    for line in (body or "").splitlines():
        m = re.fullmatch(r"\s*<!--\s*lang:(en|ru)\s*-->\s*", line)
        if m:
            current = m.group(1)
            sections.setdefault(current, [])
        else:
            sections.setdefault(current, []).append(line)
    shared = "\n".join(sections[""]).strip()
    result = {}
    for lang, other in (("en", "ru"), ("ru", "en")):
        text = "\n".join(sections.get(lang) or sections.get(other) or []).strip()
        result[lang] = (shared + "\n\n" + text).strip() if shared and text and lang in sections else (text or shared)
    return result


def fetch_text(url: str) -> str:
    req = urllib.request.Request(url, headers={"User-Agent": "am-mod-installer"})
    with urllib.request.urlopen(req, timeout=15) as response:
        return response.read(262145)[:262144].decode("utf-8", "replace")


def release_notes_by_language(release: dict, fetch=fetch_text) -> dict[str, str]:
    """Per-language notes: docs/RELEASE_NOTES-<tag>[.ru].md in the repo at that
    tag if published, else the release body split by language markers."""
    notes = split_release_body(release.get("body", ""))
    tag = release.get("tag_name") or ""
    if tag:
        for lang, suffix in (("en", ".md"), ("ru", ".ru.md")):
            url = (f"https://raw.githubusercontent.com/{REPO}/{urllib.parse.quote(tag, safe='')}"
                   f"/docs/RELEASE_NOTES-{urllib.parse.quote(tag, safe='')}{suffix}")
            try:
                text = fetch(url).strip()
            except (OSError, ValueError):
                continue
            if text:
                notes[lang] = text
    return notes


def language_notice(text: str, lang: str) -> str:
    """Tell the reader when the requested language is not published."""
    if not text:
        return text
    cyrillic = sum("\u0400" <= c <= "\u04ff" for c in text)
    letters = sum(c.isalpha() for c in text) or 1
    russian = cyrillic / letters > 0.3
    if lang == "ru" and not russian:
        return "[Русская версия описания не опубликована; показан английский текст.]\n\n" + text
    if lang == "en" and russian:
        return "[English release notes are not published; showing the Russian text.]\n\n" + text
    return text


def has_mod_asset(release: dict) -> bool:
    return any(a.get("name", "").startswith(("am-mod-", "mpc-sample-")) and a["name"].endswith(".zip")
               for a in release.get("assets", []))


def pick_mod_release(releases: list) -> dict | None:
    """Newest published stable release that carries a mod package. Releases
    that only hold the installer (or drafts/pre-releases) are skipped."""
    for release in releases:
        if release.get("draft") or release.get("prerelease"):
            continue
        if has_mod_asset(release):
            return release
    return None


def _api(path: str):
    req = urllib.request.Request(f"https://api.github.com/repos/{REPO}/{path}",
                                 headers={"Accept": "application/vnd.github+json",
                                          "User-Agent": "am-mod-installer"})
    with urllib.request.urlopen(req, timeout=30) as response:
        return json.load(response)


def release_info(tag: str = "latest") -> dict:
    """Fetch notes without downloading a package. "latest" means the newest
    release that has a mod package, not simply the newest release."""
    if tag != "latest":
        return _api("releases/tags/" + urllib.parse.quote(tag, safe=""))
    release = pick_mod_release(_api("releases?per_page=30"))
    if release is None:
        raise ValueError("No GitHub release with an am-mod-*.zip package was found.")
    return release


def download_release(rel: dict, log=print) -> Package:
    assets = [a for a in rel.get("assets", [])
              if a["name"].startswith(("am-mod-", "mpc-sample-")) and a["name"].endswith(".zip")]
    if not assets:
        raise InstallError(f"Release {rel.get('tag_name')} has no am-mod-*.zip asset.")
    asset = assets[0]
    log(f"Downloading {asset['name']} ({asset['size'] // 1024} KB) from release {rel.get('tag_name')}...")
    req = urllib.request.Request(asset["browser_download_url"], headers={"User-Agent": "am-mod-installer"})
    with urllib.request.urlopen(req, timeout=120) as response:
        data = response.read(MAX_PACKAGE + 1)
    return Package(data, asset["name"])


def latest_release(log=print) -> Package:
    """Download the latest stable mod package."""
    return download_release(release_info(), log)


# Only launcher diagnostics and installer metadata are collected.
DIAGNOSTIC_FILES = (
    VERSION_FILE,
    "autorun/MPC_MOD_EVENTS.txt",
    "autorun/MPC_CLONE_TRIAL.txt",
    "autorun/MPC_CLONE_ALREADY_ATTEMPTED.txt",
)
MAX_LOG = 2 << 20


def diagnostic_files(card: Card | None) -> tuple[dict[str, bytes], list[str]]:
    files, warnings = {}, []
    if card is None:
        return files, ["No SD card selected. Only the report and installer log will be included."]
    root = card.root.resolve()
    for name in DIAGNOSTIC_FILES:
        path = root / name
        try:
            if path.is_symlink() or path.parent.is_symlink():
                warnings.append(f"Skipped link: {name}")
                continue
            if not path.exists():
                continue
            if not path.is_file() or root not in path.resolve().parents:
                warnings.append(f"Skipped non-regular file: {name}")
                continue
            with path.open("rb") as source:
                size = os.fstat(source.fileno()).st_size
                source.seek(max(0, size - MAX_LOG))
                data = source.read(MAX_LOG)
            if size > MAX_LOG:
                data = b"[Earlier log content omitted; last 2 MiB follow]\n" + data
            files["sd/" + name] = data
        except OSError as error:
            warnings.append(f"Could not read {name}: {error.strerror or type(error).__name__}")
    if not any(name.endswith(".txt") for name in files):
        warnings.append("No MPC journals found. Reproduce the problem on the MPC, then reconnect its SD card.")
    return files, warnings


def bug_report(title: str, steps: str, expected: str, actual: str,
               card: Card | None, revision: str) -> str:
    return (f"# {title.strip()}\n\n"
            f"Mod: {revision}\nFirmware: 1.3.0.12 (installer target; confirm on the MPC)\n"
            f"Installer: {__version__} / {platform.system()} {platform.release()}\n"
            f"SD filesystem: {card.fs if card else 'not selected'}\n\n"
            f"## Steps to reproduce\n{steps.strip()}\n\n"
            f"## Expected behaviour\n{expected.strip()}\n\n"
            f"## Actual behaviour\n{actual.strip()}\n")


def issue_url(title: str, report: str) -> str:
    # Keep the URL short enough for browser/OS handoff. Full text is in report.md.
    suffix = "\n\nFull report: report.md in the diagnostic ZIP. Review and attach the ZIP before submitting."
    title = "[Bug] " + title.strip()[:80]
    body = report[:1000]
    while True:
        url = GITHUB_URL + "/issues/new?" + urllib.parse.urlencode({"title": title, "body": body + suffix})
        if len(url) <= 1900:
            return url
        body = body[:-100]



def save_diagnostics(destination: Path, report: str, files: dict[str, bytes],
                     installer_log: str, warnings: list[str]) -> None:
    """Write a local reviewable ZIP. Opening/submitting GitHub is a separate step."""
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("report.md", report)
        archive.writestr("installer.log", installer_log[-262144:])
        archive.writestr("collection.txt", "\n".join(warnings) or "All available diagnostic files collected.")
        for name, data in files.items():
            archive.writestr(name, data)


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


def has_boot_thingy(root: Path) -> bool:
    mark = Path(root) / BOOT_MARK
    return mark.is_file() and not mark.is_symlink()


def has_dsb_boot(root: Path) -> bool:
    a = Path(root) / AUTORUN
    markers = [a / "dsb-boot.conf", a / "boot/boot.sh", a / "boot/hsboot"]
    # A partial or redirected D.S.B tree must not be mistaken for a plain card.
    if not any(p.exists() or p.is_symlink() for p in (markers[0], markers[2])):
        return False
    if not all(p.is_file() and not p.is_symlink() for p in markers):
        raise InstallError("D.S.B Boot markers are incomplete or redirected; restore its boot menu first. Nothing was written.")
    if b"HSBOOT_DIR" not in markers[1].read_bytes():
        raise InstallError("Unrecognised D.S.B Boot launcher; nothing was written.")
    return True


def installation_mode(root: Path) -> str:
    dsb, thingy = has_dsb_boot(root), has_boot_thingy(root)
    if dsb and thingy:
        raise InstallError("Both D.S.B Boot and Boot Thingy markers are present; choose one boot menu first. Nothing was written.")
    return "dsb-boot" if dsb else "boot-thingy-app" if thingy else "direct"


def payload_dir(root: Path, app_mode=False) -> Path:
    mode = "boot-thingy-app" if app_mode is True else app_mode
    relative = DSB_DIR if mode == "dsb-boot" else APP_DIR + "/autorun" if mode == "boot-thingy-app" else AUTORUN
    target, card = Path(root) / relative, Path(root).resolve()
    cursor = target
    while cursor != Path(root):
        if cursor.is_symlink() or (cursor.exists() and not cursor.is_dir()):
            raise InstallError(f"{cursor} is not a regular directory; nothing was written.")
        cursor = cursor.parent
    try:
        target.resolve().relative_to(card)
    except ValueError:
        raise InstallError("Installation target leaves the selected card; nothing was written.")
    return target



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
    mode = installation_mode(root)
    app_mode = mode == "boot-thingy-app"
    dsb_mode = mode == "dsb-boot"
    a = payload_dir(root, mode)
    if dsb_mode:
        if "sampleboot.inplace" not in pkg.files or b"BOOTLOADER_START_CLEAN_STOCK" not in pkg.files["autorun.sh"]:
            raise InstallError("This AM package has no D.S.B-compatible entry. Select the updated package; nothing was written.")
        log(f"D.S.B Boot found: install AM Mod into {DSB_DIR}; boot menu and other systems are preserved.")
    if app_mode:
        log("mpc-boot-thingy found on the card: AM Mod is installed as a boot-menu app "
            f"({APP_DIR}); the boot menu itself is not changed. Choose AM Mod in the boot menu to start it.")
        if (root / AUTORUN / "autorun.sh").is_file() and b"mod-launcher.sh" in (root / AUTORUN / "autorun.sh").read_bytes():
            log("Warning: autorun/autorun.sh on this card already starts the mod directly, not the boot menu. "
                "Reinstall mpc-boot-thingy to restore its boot menu.")
    existing = sorted(p.name for p in a.iterdir()) if a.is_dir() else []
    foreign = [n for n in existing if n not in pkg.files] if existing and not (root / VERSION_FILE).exists() and mode == "direct" else []
    if foreign:
        log(f"Warning: autorun/ holds files from another package ({', '.join(foreign[:5])}...). "
            "They will be moved to the backup.")
    if dry_run:
        log("Dry run: nothing written.")
        return {}
    backup = backup_card(root, log)
    a.mkdir(parents=True, exist_ok=True)
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
        log(f"  wrote {a.relative_to(root).as_posix()}/{n}")
    if app_mode:
        _write(root / APP_DIR / "boot.entry", APP_ENTRY.encode("utf-8"))
        log(f"  wrote {APP_DIR}/boot.entry")
    managed = dict(pkg.files)
    if dsb_mode:
        label = f"AM Mod\n{pkg.revision}\n".encode("utf-8")
        _write(a / "dsb-record.txt", label)
        managed["dsb-record.txt"] = label
    info = {
        "revision": pkg.revision,
        "package": pkg.name,
        "package_sha256": pkg.sha256,
        "payload_sha256": {n: sha256(b) for n, b in sorted(managed.items())},
        "installed_at": _dt.datetime.now().astimezone().isoformat(timespec="seconds"),
        "installer": f"mpc_mod_installer {__version__} ({platform.system()})",
        "backup": str(backup) if backup else None,
    }
    if mode != "direct":
        info["mode"] = mode
        info["app_dir"] = DSB_DIR if dsb_mode else APP_DIR
    _write(root / VERSION_FILE, (json.dumps(info, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    _sync(root)
    log("Verifying...")
    for n, b in pkg.files.items():
        if file_sha256(a / n) != sha256(b):
            raise InstallError(f"{a.relative_to(root).as_posix()}/{n} on the card does not match the package. Re-insert the card and try again.")
    log(f"Done: {pkg.revision} is on {root}. Eject the card safely before removing it.")
    return info


def remove(card: Card, log=print) -> None:
    root = card.root
    v = root / VERSION_FILE
    if not v.exists():
        raise InstallError(f"No {VERSION_FILE} on {root}: no mod installed by this tool.")
    info = json.loads(v.read_text(encoding="utf-8-sig"))
    backup_card(root, log)
    app_mode = info.get("mode") == "boot-thingy-app"
    a = payload_dir(root, info.get("mode", "direct"))
    for n in info.get("payload_sha256", {}):
        p = a / PurePosixPath(n).name
        if p.is_file() and not p.is_symlink():
            os.chmod(p, 0o666)
            p.unlink()
    if a.is_dir() and not any(a.iterdir()):
        a.rmdir()
    if app_mode:
        entry = root / APP_DIR / "boot.entry"
        if entry.is_file() and not entry.is_symlink():
            os.chmod(entry, 0o666)
            entry.unlink()
        app = root / APP_DIR
        if app.is_dir() and not any(app.iterdir()):
            app.rmdir()
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
    base = payload_dir(card.root, info.get("mode", "direct"))
    bad = [n for n, h in info.get("payload_sha256", {}).items()
           if not (base / n).is_file() or file_sha256(base / n) != h]
    log(f"{card.root}: {info.get('revision')} installed {info.get('installed_at')}; "
        + ("(D.S.B Boot) " if info.get("mode") == "dsb-boot" else "(boot-menu app) " if info.get("mode") == "boot-thingy-app" else "")
        + ("all files intact." if not bad else f"{len(bad)} files changed or missing: {', '.join(bad[:5])}"))


# --------------------------------------------------------------------------
# GUI


RU = {
    "1. Mod package (.zip):": "1. Пакет мода (.zip):",
    "not selected": "не выбран",
    "2. SD card:": "2. SD-карта:",
    "Language:": "Язык:",
    "GitHub": "GitHub",
    "All releases": "Все релизы",
    "Report a bug...": "Сообщить об ошибке...",
    "Open .zip...": "Открыть .zip...",
    "Latest release": "Последний релиз",
    "Refresh": "Обновить",
    "Remove mod": "Удалить мод",
    "Install": "Установить",
    "Release notes": "О релизе",
    "Installation log": "Журнал установки",
    "Refresh news": "Обновить новости",
    "Download this release": "Скачать этот релиз",
    "Release news unavailable": "Новости релиза недоступны",
    "Check your connection, or open All releases in a browser.\nNotes from a local ZIP are available after selecting it.":
        "Проверьте соединение или откройте «Все релизы» в браузере.\nОписание из локального ZIP появится после его выбора.",
    "No release notes available. Open All releases on GitHub.": "Описания релиза нет. Откройте «Все релизы» на GitHub.",
    "Selected package: ": "Выбранный пакет: ",
    "Latest release: ": "Последний релиз: ",
    "Release notes will appear here.": "Здесь появится описание релиза.",
    "Mod package": "Пакет мода",
    "Package": "Пакет",
    "Choose a package and a card first.": "Сначала выберите пакет и карту.",
    "Install": "Установить",
    "Remove": "Удалить",
    "Report a bug": "Сообщение об ошибке",
    "Summary": "Краткое описание",
    "Installed mod version": "Установленная версия мода",
    "Steps to reproduce": "Шаги воспроизведения",
    "Expected behaviour": "Ожидаемое поведение",
    "Actual behaviour": "Фактическое поведение",
    "Include available SD journals and installer log": "Приложить журналы SD и журнал установщика",
    "Save diagnostic ZIP...": "Сохранить диагностический ZIP...",
    "Copy report": "Копировать отчёт",
    "Open GitHub issue": "Открыть issue на GitHub",
    "Close": "Закрыть",
    "Report": "Отчёт",
    "Fill in the summary, steps, expected and actual behaviour.": "Заполните описание, шаги, ожидаемое и фактическое поведение.",
    "Review the ZIP before sharing. Logs may contain project names and file paths.\nSave the ZIP, then attach it to the GitHub issue and submit in your browser.":
        "Проверьте ZIP перед отправкой: журналы могут содержать названия проектов и пути.\nСохраните ZIP, приложите его к issue на GitHub и отправьте в браузере.",
    "Full report copied. Paste it into GitHub and attach the saved ZIP.": "Отчёт скопирован. Вставьте его в GitHub и приложите сохранённый ZIP.",
    "No removable card found. Insert the SD card and press Refresh.": "Съёмная карта не найдена. Вставьте SD-карту и нажмите «Обновить».",
    "Firmware 1.3.0.12 only. The mod runs a patched copy of the stock app in RAM; it does not flash the MPC.":
        "Только прошивка 1.3.0.12. Мод запускает исправленную копию штатного приложения в RAM и не прошивает MPC.",
    "SD: exFAT or experimental FAT32/MS-DOS (FAT32). FAT32 requires package r26e or later. Volume name: MPC, for example; no spaces or Cyrillic.":
        "SD: exFAT или экспериментальный FAT32. Для FAT32 нужен пакет r26e или новее. Имя тома, например MPC: без пробелов и кириллицы.",
    "Start: insert the card and power on, or insert it after the normal screen appears. The mod starts automatically.":
        "Запуск: вставьте карту и включите MPC либо вставьте её после появления обычного экрана. Мод стартует автоматически.",
    "Exit: save your project, then use the MPC power button. Older packages may also offer Project > Mod Off.":
        "Выход: сохраните проект и используйте кнопку питания MPC. В старых пакетах есть также Project > Mod Off.",
}


def tr(text: str, lang: str) -> str:
    return RU.get(text, text) if lang == "ru" else text


def default_language() -> str:
    import locale
    try:
        code = locale.getlocale()[0] or os.environ.get("LANG", "")
    except (ValueError, TypeError):
        code = os.environ.get("LANG", "")
    return "ru" if str(code).lower().startswith(("ru", "russian")) else "en"


def gui() -> None:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    win = tk.Tk()
    win.title(f"AM Mod installer {__version__}")
    win.minsize(800, 640)
    state = {"pkg": None, "cards": [], "release": None, "news": None, "lang": default_language()}
    labelled: list = []

    def T(text: str) -> str:
        return tr(text, state["lang"])

    def L(widget, text: str):
        """Register a widget whose text follows the selected language."""
        labelled.append((widget, text))
        widget.configure(text=T(text))
        return widget

    frm = ttk.Frame(win, padding=12)
    frm.pack(fill="both", expand=True)
    frm.columnconfigure(1, weight=1)

    L(ttk.Label(frm), "1. Mod package (.zip):").grid(row=0, column=0, sticky="w")
    pkg_var = tk.StringVar(value="")
    pkg_label = ttk.Label(frm, textvariable=pkg_var)
    pkg_label.grid(row=0, column=1, sticky="w", padx=6)
    bar = ttk.Frame(frm)
    bar.grid(row=0, column=2, sticky="e")

    L(ttk.Label(frm), "2. SD card:").grid(row=1, column=0, sticky="w", pady=(10, 0))
    card_var = tk.StringVar()
    combo = ttk.Combobox(frm, textvariable=card_var, state="readonly")
    combo.grid(row=1, column=1, sticky="ew", padx=6, pady=(10, 0))

    links = ttk.Frame(frm)
    links.grid(row=2, column=0, columnspan=3, sticky="w", pady=(10, 0))
    ttk.Button(links, text="GitHub", command=lambda: webbrowser.open(GITHUB_URL)).pack(side="left")
    L(ttk.Button(links, command=lambda: webbrowser.open(GITHUB_URL + "/releases")), "All releases").pack(side="left", padx=6)

    lang_box = ttk.Frame(frm)
    lang_box.grid(row=2, column=2, sticky="e", pady=(10, 0))
    L(ttk.Label(lang_box), "Language:").pack(side="left", padx=(0, 6))
    lang_var = tk.StringVar(value="Русский" if state["lang"] == "ru" else "English")
    lang_combo = ttk.Combobox(lang_box, textvariable=lang_var, state="readonly",
                              values=["English", "Русский"], width=10)
    lang_combo.pack(side="left")

    # Release notes and the installation log are always visible together,
    # so Install/Remove never happens without the log in view.
    panes = ttk.PanedWindow(frm, orient="vertical")
    panes.grid(row=3, column=0, columnspan=3, sticky="nsew", pady=10)
    frm.rowconfigure(3, weight=1)
    news_tab = ttk.Frame(panes)
    install_tab = ttk.Frame(panes)
    panes.add(news_tab, weight=3)
    panes.add(install_tab, weight=2)
    install_tab.rowconfigure(1, weight=1)
    install_tab.columnconfigure(0, weight=1)
    log_title = L(ttk.Label(install_tab), "Installation log")
    log_title.grid(row=0, column=0, sticky="w", pady=(6, 2))
    out = tk.Text(install_tab, height=8, wrap="word", state="disabled")
    out.grid(row=1, column=0, sticky="nsew")
    scroll = ttk.Scrollbar(install_tab, command=out.yview)
    scroll.grid(row=1, column=1, sticky="ns")
    out.configure(yscrollcommand=scroll.set)
    news_tab.rowconfigure(1, weight=1)
    news_tab.columnconfigure(0, weight=1)
    news_title = tk.StringVar()
    ttk.Label(news_tab, textvariable=news_title).grid(row=0, column=0, sticky="w", pady=(0, 2))
    notes = tk.Text(news_tab, height=10, wrap="word", state="disabled")
    notes.grid(row=1, column=0, sticky="nsew")
    news_scroll = ttk.Scrollbar(news_tab, command=notes.yview)
    news_scroll.grid(row=1, column=1, sticky="ns")
    notes.configure(yscrollcommand=news_scroll.set)
    news_buttons = ttk.Frame(news_tab)
    news_buttons.grid(row=2, column=0, columnspan=2, sticky="w", pady=6)

    def set_notes(title: str, body: str) -> None:
        news_title.set(title)
        notes.configure(state="normal")
        notes.delete("1.0", "end")
        notes.insert("1.0", body or T("No release notes available. Open All releases on GitHub."))
        notes.configure(state="disabled")

    def render_news() -> None:
        """Show the current news source in the selected language."""
        src, lang = state["news"], state["lang"]
        if src is None:
            set_notes(T("Release notes"), T("Release notes will appear here."))
        elif src[0] == "package":
            pkg = src[1]
            set_notes(T("Selected package: ") + pkg.revision, language_notice(pkg.notes_for(lang), lang))
        elif src[0] == "release":
            release = src[1]
            body = language_notice(release.get("_notes", split_release_body(release.get("body", "")))[lang], lang)
            set_notes(f"{T('Latest release: ')}{release.get('tag_name', '?')}  "
                      f"{release.get('published_at', '')[:10]}", body)
        else:
            set_notes(T("Release news unavailable"),
                      T("Check your connection, or open All releases in a browser.\n"
                        "Notes from a local ZIP are available after selecting it."))

    def append_log(msg: str) -> None:
        out.configure(state="normal")
        out.insert("end", msg + "\n")
        out.see("end")
        out.configure(state="disabled")

    def log(msg: str) -> None:
        win.after(0, lambda: append_log(msg))

    def refresh() -> None:
        state["cards"] = list_cards()
        combo["values"] = [str(c) for c in state["cards"]]
        if state["cards"]:
            combo.current(0)
        else:
            card_var.set("")
            log(T("No removable card found. Insert the SD card and press Refresh."))

    def show_pkg_label() -> None:
        p = state["pkg"]
        pkg_var.set(f"{p.name}  ({p.revision})" if p else T("not selected"))

    def set_pkg(p: Package) -> None:
        state["pkg"] = p
        state["news"] = ("package", p)
        show_pkg_label()
        log(f"Package OK: {p.name}, revision {p.revision}.")
        render_news()

    def pick() -> None:
        f = filedialog.askopenfilename(title=T("Mod package"), filetypes=[("Zip", "*.zip")])
        if f:
            try:
                set_pkg(Package.from_file(Path(f)))
            except InstallError as e:
                messagebox.showerror(T("Package"), str(e))

    def run(fn) -> None:
        def worker():
            try:
                fn()
            except (InstallError, OSError, ValueError) as e:
                error = str(e)
                log(f"ERROR: {error}")
                win.after(0, lambda message=error: messagebox.showerror("AM Mod", message))
        threading.Thread(target=worker, daemon=True).start()

    def latest() -> None:
        run(lambda: (lambda p: win.after(0, lambda: set_pkg(p)))(latest_release(log)))

    def refresh_news() -> None:
        news_refresh.configure(state="disabled")

        def fetch() -> None:
            try:
                release = release_info()
                release["_notes"] = release_notes_by_language(release)
            except (OSError, ValueError) as error:
                message = str(error)
                def failed():
                    news_refresh.configure(state="normal")
                    log("Release news unavailable: " + message)
                    if not state["pkg"]:
                        state["news"] = ("error", message)
                        render_news()
                win.after(0, failed)
                return

            def ready():
                state["release"] = release
                news_refresh.configure(state="normal")
                news_download.configure(state="normal")
                if not state["pkg"] or state["news"] is None or state["news"][0] != "package":
                    state["news"] = ("release", release)
                    render_news()
            win.after(0, ready)
        threading.Thread(target=fetch, daemon=True).start()

    def download_news() -> None:
        release = state["release"]
        if release:
            run(lambda: (lambda p: win.after(0, lambda: set_pkg(p)))(download_release(release, log)))

    news_refresh = L(ttk.Button(news_buttons, command=refresh_news), "Refresh news")
    news_refresh.pack(side="left")
    news_download = L(ttk.Button(news_buttons, command=download_news, state="disabled"), "Download this release")
    news_download.pack(side="left", padx=6)

    def selected() -> Card | None:
        i = combo.current()
        return state["cards"][i] if 0 <= i < len(state["cards"]) else None

    def do_install() -> None:
        c, p = selected(), state["pkg"]
        if not p or not c:
            messagebox.showwarning("AM Mod", T("Choose a package and a card first."))
            return
        if state["lang"] == "ru":
            question = (f"Установить {p.revision} на {c.root}?\n\nЗаписываются только autorun/ и MPC_MOD_VERSION.json; "
                        "старый autorun/ сохраняется на этом компьютере. Проекты и сэмплы не затрагиваются.")
        else:
            question = (f"Install {p.revision} on {c.root}?\n\n"
                        "Only autorun/ and MPC_MOD_VERSION.json are written; the old autorun/ is backed up "
                        "on this computer. Projects and samples are not touched.")
        try:
            mode = installation_mode(c.root)
            target = payload_dir(c.root, mode).relative_to(c.root)
        except InstallError as exc:
            messagebox.showerror("AM Mod", str(exc))
            return
        if mode == "dsb-boot":
            question = (f"Установить {p.revision} в {target}?\n\nD.S.B Boot и остальные системы сохранятся. Текущая autorun/ будет сохранена на компьютере." if state["lang"] == "ru" else
                        f"Install {p.revision} into {target}?\n\nD.S.B Boot and other systems are preserved. Current autorun/ is backed up on this computer.")
        if has_boot_thingy(c.root):
            question += ("\n\nНайден mpc-boot-thingy: мод будет установлен как приложение меню загрузки "
                         "(autorun/apps/am-mod), меню не изменится." if state["lang"] == "ru" else
                         "\n\nmpc-boot-thingy found: the mod is installed as a boot-menu app "
                         "(autorun/apps/am-mod); the boot menu is not changed.")
        if messagebox.askyesno(T("Install"), question):
            run(lambda: install(p, c, log))

    def do_remove() -> None:
        c = selected()
        question = f"Удалить мод с {c.root}?" if c and state["lang"] == "ru" else (f"Remove the mod from {c.root}?" if c else "")
        if c and messagebox.askyesno(T("Remove"), question):
            run(lambda: remove(c, log))

    def report_bug() -> None:
        dialog = tk.Toplevel(win)
        dialog.title(T("Report a bug"))
        dialog.transient(win)
        dialog.grab_set()
        panel = ttk.Frame(dialog, padding=12)
        panel.pack(fill="both", expand=True)
        panel.columnconfigure(1, weight=1)
        card = selected()
        revision = installed_revision(card.root) if card else "unknown"
        revision_var = tk.StringVar(value=revision)
        title_var = tk.StringVar()
        ttk.Label(panel, text=T("Summary")).grid(row=0, column=0, sticky="w")
        ttk.Entry(panel, textvariable=title_var, width=65).grid(row=0, column=1, sticky="ew", pady=4)
        ttk.Label(panel, text=T("Installed mod version")).grid(row=1, column=0, sticky="w")
        ttk.Entry(panel, textvariable=revision_var).grid(row=1, column=1, sticky="ew", pady=4)
        fields = []
        for row, label in enumerate((T("Steps to reproduce"), T("Expected behaviour"), T("Actual behaviour")), 2):
            ttk.Label(panel, text=label).grid(row=row, column=0, sticky="nw", pady=4)
            widget = tk.Text(panel, height=3, width=60, wrap="word")
            widget.grid(row=row, column=1, sticky="ew", pady=4)
            fields.append(widget)
        include_logs = tk.BooleanVar(value=True)
        ttk.Checkbutton(panel, text=T("Include available SD journals and installer log"),
                        variable=include_logs).grid(row=5, column=0, columnspan=2, sticky="w", pady=4)
        preview = tk.Text(panel, height=6, width=80, wrap="word", state="disabled")
        preview.grid(row=6, column=0, columnspan=2, sticky="ew", pady=4)

        def preview_files(*_args):
            files, warnings = diagnostic_files(card) if include_logs.get() else ({}, [])
            listing = ["SD: " + (str(card.root) if card else "not selected"),
                       "Archive: report.md, collection.txt, installer.log"]
            listing += [f"{name} ({len(data) // 1024} KB)" for name, data in files.items()]
            listing += warnings
            preview.configure(state="normal")
            preview.delete("1.0", "end")
            preview.insert("1.0", "\n".join(listing))
            preview.configure(state="disabled")

        include_logs.trace_add("write", preview_files)
        preview_files()
        ttk.Label(panel, text=T("Review the ZIP before sharing. Logs may contain project names and file paths.\n"
                  "Save the ZIP, then attach it to the GitHub issue and submit in your browser."),
                  wraplength=660).grid(row=7, column=0, columnspan=2, sticky="w", pady=6)
        result_var = tk.StringVar()
        ttk.Label(panel, textvariable=result_var, wraplength=660).grid(row=8, column=0, columnspan=2, sticky="w")
        prepared = {"body": "", "title": ""}

        def save_report():
            title = title_var.get().strip()
            values = [widget.get("1.0", "end").strip() for widget in fields]
            if not title or not all(values):
                messagebox.showwarning(T("Report"), T("Fill in the summary, steps, expected and actual behaviour."), parent=dialog)
                return
            filename = filedialog.asksaveasfilename(parent=dialog, title="Save diagnostic ZIP",
                       defaultextension=".zip", filetypes=[("Diagnostic ZIP", "*.zip")],
                       initialfile="am-mod-report-" + _dt.datetime.now().strftime("%Y%m%d-%H%M%S") + ".zip")
            if not filename:
                return
            try:
                files, warnings = diagnostic_files(card) if include_logs.get() else ({}, [])
                report = bug_report(title, *values, card, revision_var.get().strip() or "unknown")
                installer_log = out.get("1.0", "end") if include_logs.get() else "Excluded by user."
                save_diagnostics(Path(filename), report, files, installer_log, warnings)
            except (OSError, ValueError) as error:
                messagebox.showerror(T("Report"), str(error), parent=dialog)
                return
            prepared.update(body=report, title=title)
            result_var.set("Saved: " + filename + "\nAttach this ZIP to your issue.")
            open_issue.configure(state="normal")
            copy_report.configure(state="normal")

        def copy_text():
            dialog.clipboard_clear()
            dialog.clipboard_append(prepared["body"])
            result_var.set(T("Full report copied. Paste it into GitHub and attach the saved ZIP."))

        buttons = ttk.Frame(panel)
        buttons.grid(row=9, column=0, columnspan=2, sticky="e", pady=(8, 0))
        ttk.Button(buttons, text=T("Save diagnostic ZIP..."), command=save_report).pack(side="left")
        copy_report = ttk.Button(buttons, text=T("Copy report"), command=copy_text, state="disabled")
        copy_report.pack(side="left", padx=6)
        open_issue = ttk.Button(buttons, text=T("Open GitHub issue"), state="disabled",
                               command=lambda: webbrowser.open(issue_url(prepared["title"], prepared["body"])))
        open_issue.pack(side="left")
        ttk.Button(buttons, text=T("Close"), command=dialog.destroy).pack(side="left", padx=(6, 0))

    L(ttk.Button(links, command=report_bug), "Report a bug...").pack(side="left")
    L(ttk.Button(bar, command=pick), "Open .zip...").pack(side="left")
    L(ttk.Button(bar, command=latest), "Latest release").pack(side="left", padx=(6, 0))
    L(ttk.Button(frm, command=refresh), "Refresh").grid(row=1, column=2, sticky="e", pady=(10, 0))
    btns = ttk.Frame(frm)
    btns.grid(row=4, column=0, columnspan=3, sticky="e")
    L(ttk.Button(btns, command=do_remove), "Remove mod").pack(side="left")
    L(ttk.Button(btns, command=do_install), "Install").pack(side="left", padx=(6, 0))

    def change_language(_event=None) -> None:
        state["lang"] = "ru" if lang_var.get() == "Русский" else "en"
        for widget, text in labelled:
            widget.configure(text=T(text))
        show_pkg_label()
        render_news()

    lang_combo.bind("<<ComboboxSelected>>", change_language)

    log(T("Firmware 1.3.0.12 only. The mod runs a patched copy of the stock app in RAM; it does not flash the MPC."))
    log(T("SD: exFAT or experimental FAT32/MS-DOS (FAT32). FAT32 requires package r26e or later. Volume name: MPC, for example; no spaces or Cyrillic."))
    log(T("Start: insert the card and power on, or insert it after the normal screen appears. The mod starts automatically."))
    log(T("Exit: save your project, then use the MPC power button. Older packages may also offer Project > Mod Off."))
    show_pkg_label()
    render_news()
    refresh()
    win.after(100, refresh_news)
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

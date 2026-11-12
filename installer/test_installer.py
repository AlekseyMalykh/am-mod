"""Tests for mpc_mod_installer on a folder that stands in for an SD card."""
import hashlib
import io
import json
import os
import plistlib
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import mpc_mod_installer as m


def make_package(extra=None, sh=b"#!/bin/sh\n# revision=r99-test\n# sd-filesystems=exfat,vfat\n", corrupt=False) -> bytes:
    manifest = json.dumps({"value0": {"scripts": [{"path": "autorun.sh", "askQuestion": True,
                                                   "message": "Start test", "yesButtonText": "Start",
                                                   "noButtonText": "Cancel"}], "files": []}}).encode()
    check = hashlib.sha256(manifest + sh + m.AUTORUN_CHECK_SALT).hexdigest().encode()
    files = {"autorun/autorun.json": manifest, "autorun/autorun.sh": sh + (b"#" if corrupt else b""),
             "autorun/autorun.check": check, "autorun/mod-launcher.sh": b"launcher",
             "autorun/payload.bin": os.urandom(64), "README_RU.md": b"doc"}
    files.update(extra or {})
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        for n, b in files.items():
            z.writestr(n, b)
    return buf.getvalue()


class InstallerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = Path(self.tmp.name)
        self.card = base / "card"
        (self.card / "MPC-Sample" / "Projects").mkdir(parents=True)
        (self.card / "MPC-Sample" / "Projects" / "song.xpj").write_text("mine")
        os.environ["MPC_MOD_BACKUP_DIR"] = str(base / "backups")
        self.log = []

    def tearDown(self):
        self.tmp.cleanup()

    def card_obj(self):
        return m.Card(self.card, "TEST", "exFAT", 64_000_000_000)

    def test_package_validation(self):
        p = m.Package(make_package(), "mpc-sample-r99.zip")
        self.assertEqual(p.revision, "r99-test")
        self.assertEqual(p.ordered()[-1], "autorun.check")
        with self.assertRaises(m.InstallError):
            m.Package(make_package(corrupt=True), "bad.zip")
        with self.assertRaises(m.InstallError):
            m.Package(make_package({"../evil": b"x"}), "evil.zip")
        with self.assertRaises(m.InstallError):
            m.Package(b"not a zip", "x.zip")

    def test_package_release_notes_available_offline(self):
        package = m.Package(make_package({"README.md": b"English notes",
                                         "README.ru.md": "Русские заметки".encode()}), "release.zip")
        self.assertEqual(package.notes, "Русские заметки")
        self.assertNotIn("README.ru.md", package.files)

    def test_diagnostics_only_collect_known_logs_and_preserve_card(self):
        autorun = self.card / "autorun"
        autorun.mkdir()
        (autorun / "MPC_MOD_EVENTS.txt").write_text("event log")
        (autorun / "MPC_CLONE_TRIAL.txt").write_text("native log")
        (autorun / "private.txt").write_text("private")
        before = {str(p.relative_to(self.card)): p.read_bytes()
                  for p in self.card.rglob("*") if p.is_file()}
        files, warnings = m.diagnostic_files(self.card_obj())
        destination = Path(self.tmp.name) / "diagnostics.zip"
        report = m.bug_report("Ошибка", "1. Open Mixer", "Sound", "Silence", self.card_obj(), "r28d")
        m.save_diagnostics(destination, report, files, "installer output", warnings)
        with zipfile.ZipFile(destination) as archive:
            self.assertIsNone(archive.testzip())
            self.assertEqual(set(archive.namelist()),
                             {"report.md", "collection.txt", "installer.log",
                              "sd/autorun/MPC_MOD_EVENTS.txt", "sd/autorun/MPC_CLONE_TRIAL.txt"})
            self.assertIn("Ошибка", archive.read("report.md").decode())
        self.assertEqual(before, {str(p.relative_to(self.card)): p.read_bytes()
                                 for p in self.card.rglob("*") if p.is_file()})

    def test_diagnostics_bound_large_logs_and_handle_missing_card(self):
        autorun = self.card / "autorun"
        autorun.mkdir()
        (autorun / "MPC_MOD_EVENTS.txt").write_bytes(b"x" * (m.MAX_LOG + 10) + b"LAST")
        files, warnings = m.diagnostic_files(self.card_obj())
        data = files["sd/autorun/MPC_MOD_EVENTS.txt"]
        self.assertTrue(data.endswith(b"LAST"))
        self.assertLess(len(data), m.MAX_LOG + 100)
        self.assertEqual(m.diagnostic_files(None)[0], {})
        self.assertTrue(m.diagnostic_files(None)[1])

    @unittest.skipIf(os.name == "nt", "symlink creation needs privileges on Windows")
    def test_diagnostics_skip_symlinks(self):
        autorun = self.card / "autorun"
        autorun.mkdir()
        private = Path(self.tmp.name) / "private.txt"
        private.write_text("secret")
        (autorun / "MPC_MOD_EVENTS.txt").symlink_to(private)
        files, warnings = m.diagnostic_files(self.card_obj())
        self.assertFalse(files)
        self.assertTrue(any("link" in warning for warning in warnings))

    def test_issue_url_keeps_unicode_reports_within_browser_limit(self):
        from urllib.parse import parse_qs, urlparse
        report = "Очень длинное описание ошибки " * 1000
        url = m.issue_url("Новая ошибка " * 100, report)
        self.assertLessEqual(len(url), 1900)
        parsed = urlparse(url)
        self.assertEqual(parsed.netloc, "github.com")
        query = parse_qs(parsed.query)
        self.assertTrue(query["title"][0].startswith("[Bug] Новая"))
        self.assertIn("report.md", query["body"][0])

    def test_install_backup_remove(self):
        (self.card / "autorun").mkdir()
        (self.card / "autorun" / "other.sh").write_text("someone else's package")
        pkg = m.Package(make_package(), "mpc-sample-r99.zip")
        info = m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(sorted(p.name for p in (self.card / "autorun").iterdir()), sorted(pkg.files))
        self.assertTrue(Path(info["backup"], "autorun", "other.sh").exists())
        self.assertEqual((self.card / "MPC-Sample" / "Projects" / "song.xpj").read_text(), "mine")
        self.assertEqual(m.installed_revision(self.card), "r99-test")
        m.remove(self.card_obj(), self.log.append)
        self.assertFalse((self.card / "autorun").exists())
        self.assertFalse((self.card / m.VERSION_FILE).exists())
        self.assertEqual((self.card / "MPC-Sample" / "Projects" / "song.xpj").read_text(), "mine")

    def test_reinstall_and_dry_run(self):
        pkg = m.Package(make_package(), "mpc-sample-r99.zip")
        m.install(pkg, self.card_obj(), self.log.append)
        m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(len(list((self.card / "autorun").iterdir())), len(pkg.files))
        m.remove(self.card_obj(), self.log.append)
        m.install(pkg, self.card_obj(), self.log.append, dry_run=True)
        self.assertFalse((self.card / "autorun").exists())

    def test_rejects_system_disk(self):
        with self.assertRaises(m.InstallError):
            m.check_card(m.Card(Path(Path.home().anchor)), self.log.append)

    def test_rejects_incompatible_cards_before_backup_or_write(self):
        pkg = m.Package(make_package(), "am-mod-r99.zip")
        (self.card / "autorun").mkdir()
        original = self.card / "autorun" / "existing.sh"
        original.write_bytes(b"original autorun")
        invalid = [(fs, "MPC") for fs in ("FAT", "NTFS", "ext4", "APFS", "fuseblk", "")]
        invalid += [("exFAT", label) for label in ("", "MPC SAMPLE", "Карта", "MPC%", "MPC\n")]
        for fs, label in invalid:
            with self.subTest(fs=fs, label=label), patch.object(m, "backup_card") as backup, \
                    patch.object(m, "_write") as write:
                with self.assertRaises(m.InstallError):
                    m.install(pkg, m.Card(self.card, label, fs), self.log.append)
                backup.assert_not_called()
                write.assert_not_called()
                self.assertEqual(original.read_bytes(), b"original autorun")
                self.assertEqual((self.card / "MPC-Sample/Projects/song.xpj").read_text(), "mine")
                self.assertFalse((self.card / m.VERSION_FILE).exists())

    def test_accepts_exfat_fat32_and_supported_volume_names(self):
        for fs in ("exfat", "exFAT", "EXFAT", "FAT32", "fat32", "vfat", "msdos"):
            for label in ("MPC", "MPCSAMPLE", "Mpc-01_test.card"):
                with self.subTest(fs=fs, label=label):
                    m.check_card(m.Card(self.card, label, fs), self.log.append)

    def test_mac_reads_actual_filesystem_and_volume_name(self):
        for fs in ("exfat", "msdos"):
            info = plistlib.dumps({"MountPoint": str(self.card), "FilesystemType": fs,
                                  "VolumeName": "MPC", "TotalSize": 64_000_000_000})
            with self.subTest(fs=fs), patch.object(m.subprocess, "run", return_value=SimpleNamespace(stdout=info)) as run:
                card = m._mac_card(self.card)
                run.assert_called_once_with(["diskutil", "info", "-plist", str(self.card)],
                                            capture_output=True, check=True, timeout=10)
                self.assertEqual((card.fs, card.label), (fs, "MPC"))
                m.check_card(card)

    def test_fat32_install_backup_reinstall_remove_preserves_user_files(self):
        pkg = m.Package(make_package(), "am-mod-r99.zip")
        for fs in ("FAT32", "vfat", "msdos"):
            with self.subTest(fs=fs):
                card = m.Card(self.card, "MPC", fs)
                m.install(pkg, card, self.log.append)
                info = m.install(pkg, card, self.log.append)
                self.assertTrue(Path(info["backup"], "autorun/autorun.sh").is_file())
                m.remove(card, self.log.append)
                self.assertEqual((self.card / "MPC-Sample/Projects/song.xpj").read_text(), "mine")
                self.assertFalse((self.card / "autorun").exists())

    def test_legacy_package_rejected_on_fat32_before_backup_or_write(self):
        pkg = m.Package(make_package(sh=b"#!/bin/sh\n# revision=r26d\n"), "am-mod-r26d.zip")
        for fs in ("FAT32", "vfat", "msdos"):
            with self.subTest(fs=fs), patch.object(m, "backup_card") as backup, patch.object(m, "_write") as write:
                with self.assertRaisesRegex(m.InstallError, "r26e or later"):
                    m.install(pkg, m.Card(self.card, "MPC", fs), self.log.append)
                backup.assert_not_called()
                write.assert_not_called()
        m.install(pkg, self.card_obj(), self.log.append)

    def test_invalid_filesystem_declaration(self):
        with self.assertRaisesRegex(m.InstallError, "Invalid SD filesystem"):
            m.Package(make_package(sh=b"#!/bin/sh\n# sd-filesystems=ext4\n"), "bad.zip")

    def test_mac_detection_failure_cannot_bypass_checks(self):
        failures = (OSError("diskutil unavailable"), subprocess.TimeoutExpired("diskutil", 10),
                    subprocess.CalledProcessError(1, "diskutil"))
        for failure in failures:
            with self.subTest(failure=failure), patch.object(m.subprocess, "run", side_effect=failure):
                with self.assertRaisesRegex(m.InstallError, "Cannot verify the filesystem"):
                    m.check_card(m._mac_card(self.card))
        for data in (b"bad plist", plistlib.dumps({}), plistlib.dumps({"MountPoint": "/other"})):
            with self.subTest(data=data), patch.object(m.subprocess, "run", return_value=SimpleNamespace(stdout=data)):
                with self.assertRaisesRegex(m.InstallError, "Cannot verify the filesystem"):
                    m.check_card(m._mac_card(self.card))

    def test_mac_card_discovery_uses_volume_metadata(self):
        with patch.object(m.sys, "platform", "darwin"), patch.object(Path, "exists", return_value=True), \
                patch.object(Path, "iterdir", return_value=iter([self.card])), \
                patch.object(Path, "is_dir", return_value=True), patch.object(Path, "is_symlink", return_value=False), \
                patch.object(m.os.path, "ismount", return_value=True), \
                patch.object(m, "_mac_card", return_value=self.card_obj()) as metadata:
            self.assertEqual(len(m._posix_cards()), 1)
            metadata.assert_called_once_with(self.card)

    def test_unlisted_path_cannot_bypass_metadata_checks(self):
        with patch.object(m, "list_cards", return_value=[]):
            with self.assertRaisesRegex(m.InstallError, "Cannot verify the filesystem"):
                m.card_from_arg(str(self.card))

    def test_linux_uses_device_label_not_mount_directory_name(self):
        with patch.object(m.subprocess, "run", return_value=SimpleNamespace(stdout="MPC SAMPLE\n")):
            label = m._linux_label("/dev/test-card1")
        self.assertEqual(label, "MPC SAMPLE")
        with self.assertRaisesRegex(m.InstallError, "volume name"):
            m.check_card(m.Card(self.card, label, "exfat"))
        with patch.object(m.subprocess, "run", side_effect=OSError("lsblk unavailable")):
            self.assertEqual(m._linux_label("/dev/test-card1"), "")

    def test_linux_mount_paths_preserve_unicode_and_decode_spaces(self):
        mounts = "/dev/test1 /media/user/Карта\\0401 exfat rw,nosuid 0 0\n"
        with patch.object(m.sys, "platform", "linux"), patch.object(Path, "read_text", return_value=mounts), \
                patch.object(m.os, "statvfs", return_value=SimpleNamespace(f_blocks=100, f_frsize=4096), create=True), \
                patch.object(m, "_linux_label", return_value="MPC"):
            cards = m._posix_cards()
        self.assertEqual(str(cards[0].root), str(Path("/media/user/Карта 1")))
        self.assertEqual((cards[0].fs, cards[0].label), ("exfat", "MPC"))

    @unittest.skipUnless(sys.platform == "darwin", "native macOS volume metadata")
    def test_native_mac_metadata(self):
        card = m._mac_card(Path("/"))
        self.assertTrue(card.fs, "diskutil must identify the actual filesystem")
        self.assertTrue(card.label, "diskutil must identify the actual volume name")

    @unittest.skipUnless(os.name == "nt", "native Windows volume metadata")
    def test_native_windows_metadata(self):
        card = m._windows_card(Path(Path.home().anchor))
        self.assertIsNotNone(card)
        self.assertTrue(card.fs, "GetVolumeInformationW must identify the actual filesystem")


if __name__ == "__main__":
    unittest.main()

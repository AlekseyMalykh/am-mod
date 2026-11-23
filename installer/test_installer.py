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
        self.assertEqual(package.notes_for("en"), "English notes")
        self.assertEqual(package.notes_for("ru"), "Русские заметки")
        self.assertNotIn("README.ru.md", package.files)

    def test_release_body_is_split_by_language(self):
        body = "Shared\n<!-- lang:en -->\nEnglish text\n<!-- lang:ru -->\nРусский текст"
        parts = m.split_release_body(body)
        self.assertEqual(parts["en"], "Shared\n\nEnglish text")
        self.assertEqual(parts["ru"], "Shared\n\nРусский текст")
        plain = m.split_release_body("Only one text")
        self.assertEqual(plain["en"], plain["ru"])
        only_en = m.split_release_body("<!-- lang:en -->\nEN only")
        self.assertEqual(only_en["ru"], "EN only")

    def test_language_notice_and_per_language_docs(self):
        self.assertTrue(m.language_notice("English only", "ru").startswith("[Русская версия"))
        self.assertEqual(m.language_notice("Только русский текст", "ru"), "Только русский текст")
        self.assertTrue(m.language_notice("Только русский текст", "en").startswith("[English"))
        def fake(url):
            if url.endswith("RELEASE_NOTES-r1.ru.md"): return "Русские заметки релиза"
            if url.endswith("RELEASE_NOTES-r1.md"): return "English release notes"
            raise OSError("404")
        notes = m.release_notes_by_language({"tag_name": "r1", "body": "body"}, fake)
        self.assertEqual((notes["en"], notes["ru"]), ("English release notes", "Русские заметки релиза"))
        offline = m.release_notes_by_language({"tag_name": "r1", "body": "body"}, lambda u: (_ for _ in ()).throw(OSError()))
        self.assertEqual(offline["ru"], "body")

    def test_latest_skips_installer_only_releases(self):
        mod = {"tag_name": "r1", "assets": [{"name": "am-mod-r1.zip", "size": 1, "browser_download_url": "x"}]}
        installer = {"tag_name": "installer-1.2.0", "assets": [{"name": "MPC-Sample-Mod-Installer.exe"}]}
        draft = dict(mod, tag_name="d", draft=True)
        pre = dict(mod, tag_name="p", prerelease=True)
        self.assertIs(m.pick_mod_release([installer, draft, pre, mod]), mod)
        self.assertIsNone(m.pick_mod_release([installer]))

    def test_every_gui_translation_key_is_used_and_nonempty(self):
        self.assertEqual(m.tr("Install", "ru"), "Установить")
        self.assertEqual(m.tr("Install", "en"), "Install")
        self.assertEqual(m.tr("Unknown text", "ru"), "Unknown text")
        self.assertTrue(all(v.strip() for v in m.RU.values()))

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

    def test_boot_thingy_card_gets_app_install_and_boot_menu_is_untouched(self):
        a = self.card / "autorun"
        (a / "system").mkdir(parents=True)
        (a / "apps" / "other").mkdir(parents=True)
        files = {"system/mpc-boot-thingy.mark": b"mpc-boot-thingy", "autorun.sh": b"boot thingy sh",
                 "autorun.json": b"{}", "autorun.check": b"abc", "splash-screen.png": b"png",
                 "apps/other/boot.entry": b"title=x\nrun=y\n"}
        for n, b in files.items():
            (a / n).write_bytes(b)
        pkg = m.Package(make_package(), "am-mod-r99.zip")
        info = m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(info["mode"], "boot-thingy-app")
        for n, b in files.items():
            self.assertEqual((a / n).read_bytes(), b)
        app = a / "apps" / "am-mod"
        self.assertEqual((app / "boot.entry").read_text(), "title=AM Mod\nrun=autorun/autorun.sh\ntakeover=1\n")
        self.assertEqual(sorted(p.name for p in (app / "autorun").iterdir()), sorted(pkg.files))
        self.assertTrue(m.has_boot_thingy(self.card))
        m.status(self.card_obj(), self.log.append)
        self.assertIn("boot-menu app", self.log[-1])
        self.assertIn("all files intact", self.log[-1])
        m.install(pkg, self.card_obj(), self.log.append)  # reinstall in place
        m.remove(self.card_obj(), self.log.append)
        self.assertFalse(app.exists())
        self.assertFalse((self.card / m.VERSION_FILE).exists())
        for n, b in files.items():
            self.assertEqual((a / n).read_bytes(), b)

    def test_card_without_boot_thingy_installs_directly(self):
        pkg = m.Package(make_package(), "am-mod-r99.zip")
        info = m.install(pkg, self.card_obj(), self.log.append)
        self.assertNotIn("mode", info)
        self.assertTrue((self.card / "autorun" / "autorun.sh").is_file())
        self.assertFalse((self.card / "autorun" / "apps").exists())

    def test_reinstall_and_dry_run(self):
        pkg = m.Package(make_package(), "mpc-sample-r99.zip")
        m.install(pkg, self.card_obj(), self.log.append)
        m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(len(list((self.card / "autorun").iterdir())), len(pkg.files))
        m.remove(self.card_obj(), self.log.append)
        m.install(pkg, self.card_obj(), self.log.append, dry_run=True)
        self.assertFalse((self.card / "autorun").exists())

    def dsb_card(self):
        a = self.card / "autorun"
        (a / "boot").mkdir(parents=True, exist_ok=True)
        (a / "dsb-boot.conf").write_text("THEME=records\n")
        (a / "boot/boot.sh").write_text("HSBOOT_DIR=card\n")
        (a / "boot/hsboot").write_bytes(b"boot binary sentinel")
        (a / "autorun.sh").write_bytes(b"HS boot root entry sentinel")
        (a / "os/SampQOL").mkdir(parents=True)
        (a / "os/SampQOL/autorun.sh").write_bytes(b"SampQOL sentinel")
        return {p.relative_to(self.card): p.read_bytes() for p in a.rglob("*") if p.is_file()}

    def dsb_package(self):
        return m.Package(make_package({"autorun/sampleboot.inplace": b""}, sh=b"#!/bin/sh\n# revision=r99-test\n# sd-filesystems=exfat,vfat\n# BOOTLOADER_START_CLEAN_STOCK\n"), "am-mod-r99.zip")

    def test_dsb_install_update_status_remove_preserves_boot_and_other_systems(self):
        boot = self.dsb_card()
        pkg = self.dsb_package()
        info = m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(info["mode"], "dsb-boot")
        target = self.card / m.DSB_DIR
        self.assertEqual((target / "payload.bin").read_bytes(), pkg.files["payload.bin"])
        self.assertIn(b"r99-test", (target / "dsb-record.txt").read_bytes())
        self.assertEqual(m.installed_revision(self.card), "r99-test")
        m.status(self.card_obj(), self.log.append)
        self.assertIn("all files intact", self.log[-1])
        m.install(pkg, self.card_obj(), self.log.append)
        m.remove(self.card_obj(), self.log.append)
        self.assertFalse(target.exists())
        for name, data in boot.items():
            self.assertEqual((self.card / name).read_bytes(), data)
        self.assertEqual((self.card / "MPC-Sample/Projects/song.xpj").read_text(), "mine")

    def test_dsb_dry_run_and_unsupported_package_do_not_write(self):
        boot = self.dsb_card()
        with patch.object(m, "backup_card") as backup:
            m.install(self.dsb_package(), self.card_obj(), self.log.append, dry_run=True)
            backup.assert_not_called()
            with self.assertRaises(m.InstallError):
                m.install(m.Package(make_package(), "old.zip"), self.card_obj(), self.log.append)
            backup.assert_not_called()
        self.assertFalse((self.card / m.DSB_DIR).exists())
        for name, data in boot.items():
            self.assertEqual((self.card / name).read_bytes(), data)

    def test_partial_dsb_markers_do_not_fall_back_to_root_install(self):
        boot = self.dsb_card()
        (self.card / "autorun/boot/hsboot").unlink()
        with patch.object(m, "backup_card") as backup, self.assertRaises(m.InstallError):
            m.install(self.dsb_package(), self.card_obj(), self.log.append)
        backup.assert_not_called()
        self.assertEqual((self.card / "autorun/autorun.sh").read_bytes(), boot[Path("autorun/autorun.sh")])

    def test_dsb_refuses_redirected_destination(self):
        self.dsb_card()
        outside = Path(self.tmp.name) / "outside"
        outside.mkdir()
        try:
            (self.card / m.DSB_DIR).symlink_to(outside, target_is_directory=True)
        except OSError:
            self.skipTest("directory symlinks unavailable")
        with patch.object(m, "backup_card") as backup, self.assertRaises(m.InstallError):
            m.install(self.dsb_package(), self.card_obj(), self.log.append)
        backup.assert_not_called()
        self.assertFalse(list(outside.iterdir()))

    def sampleboot_card(self, root_conf=False):
        a = self.card / "autorun"
        (a / "sampleboot").mkdir(parents=True, exist_ok=True)
        (a / "autorun.sh").write_bytes(b"#!/bin/sh\n# SAMPLEBOOT\nexec sh sampleboot/sampleboot.sh\n")
        (a / "sampleboot/sampleboot.sh").write_bytes(b"sampleboot script sentinel")
        (a / "sampleboot/sampleboot-ui").write_bytes(b"ui sentinel")
        (a / "sampleboot/sampleboot.conf").write_text('ROOT_FILES="mpcqol.conf"\n')
        (a / "mods/SampQOL").mkdir(parents=True)
        (a / "mods/SampQOL/autorun.sh").write_bytes(b"SampQOL sentinel")
        if root_conf:
            (self.card / "mpcqol.conf").write_text("x=1\n")
        return {p.relative_to(self.card): p.read_bytes() for p in a.rglob("*") if p.is_file()}

    def test_sampleboot_install_status_remove_preserves_boot_and_other_systems(self):
        boot = self.sampleboot_card()
        pkg = self.dsb_package()
        info = m.install(pkg, self.card_obj(), self.log.append)
        self.assertEqual(info["mode"], "sampleboot")
        target = self.card / m.SAMPLEBOOT_DIR
        self.assertEqual((target / "payload.bin").read_bytes(), pkg.files["payload.bin"])
        self.assertTrue((target / "sampleboot.inplace").is_file())
        self.assertEqual((target / "sampleboot.txt").read_bytes(), b"Am MOD\n")
        m.status(self.card_obj(), self.log.append)
        self.assertIn("(SampleBoot)", self.log[-1])
        self.assertIn("all files intact", self.log[-1])
        m.install(pkg, self.card_obj(), self.log.append)
        m.remove(self.card_obj(), self.log.append)
        self.assertFalse(target.exists())
        for name, data in boot.items():
            self.assertEqual((self.card / name).read_bytes(), data)

    def test_sampleboot_warns_about_card_root_settings_file(self):
        self.sampleboot_card(root_conf=True)
        m.install(self.dsb_package(), self.card_obj(), self.log.append)
        self.assertTrue(any("mpcqol.conf" in line and "ROOT_FILES" in line for line in self.log))

    def test_sampleboot_without_marker_is_not_mistaken_and_old_package_refused(self):
        boot = self.sampleboot_card()
        with patch.object(m, "backup_card") as backup, self.assertRaises(m.InstallError):
            m.install(m.Package(make_package(), "old.zip"), self.card_obj(), self.log.append)
        backup.assert_not_called()
        self.assertFalse((self.card / m.SAMPLEBOOT_DIR).exists())
        (self.card / "autorun/sampleboot/sampleboot-ui").unlink()
        with patch.object(m, "backup_card") as backup, self.assertRaises(m.InstallError):
            m.install(self.dsb_package(), self.card_obj(), self.log.append)
        backup.assert_not_called()

    def test_two_boot_menus_are_refused(self):
        self.dsb_card()
        a = self.card / "autorun"
        (a / "sampleboot").mkdir(exist_ok=True)
        (a / "sampleboot/sampleboot.sh").write_bytes(b"x")
        (a / "sampleboot/sampleboot-ui").write_bytes(b"x")
        (a / "autorun.sh").write_bytes(b"HSBOOT_DIR # SAMPLEBOOT")
        with patch.object(m, "backup_card") as backup, self.assertRaises(m.InstallError):
            m.install(self.dsb_package(), self.card_obj(), self.log.append)
        backup.assert_not_called()

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

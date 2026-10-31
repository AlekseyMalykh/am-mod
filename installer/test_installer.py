"""Tests for mpc_mod_installer on a folder that stands in for an SD card."""
import hashlib
import io
import json
import os
import tempfile
import unittest
import zipfile
from pathlib import Path

import mpc_mod_installer as m


def make_package(extra=None, sh=b"#!/bin/sh\n# revision=r99-test\n", corrupt=False) -> bytes:
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


if __name__ == "__main__":
    unittest.main()

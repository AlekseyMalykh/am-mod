from __future__ import annotations

import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "testapp_probe"
OUTPUT = ROOT / "mpc-mod-probe-testapp.zip"


def add_file(archive: zipfile.ZipFile, path: Path, executable: bool = False) -> None:
    info = zipfile.ZipInfo(path.name)
    info.create_system = 3
    mode = 0o100755 if executable else 0o100644
    info.external_attr = mode << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, path.read_bytes())


with zipfile.ZipFile(OUTPUT, "w") as archive:
    add_file(archive, SOURCE / "mpc-probe-testapp", executable=True)
    add_file(archive, SOURCE / "probe.lua")

print(OUTPUT)

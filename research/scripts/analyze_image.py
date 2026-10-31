from __future__ import annotations

import hashlib
import mmap
import re
import struct
import sys
from pathlib import Path


MAGICS = {
    b"ANDROID!": "Android boot image",
    b"\x27\x05\x19\x56": "U-Boot uImage (big-endian)",
    b"\x56\x19\x05\x27": "U-Boot uImage (little-endian bytes)",
    b"\xd0\x0d\xfe\xed": "Flattened Device Tree / FIT",
    b"hsqs": "SquashFS little-endian",
    b"sqsh": "SquashFS big-endian",
    b"UBI#": "UBI erase-count header",
    b"\x7fELF": "ELF",
    b"070701": "cpio newc",
    b"ustar": "tar",
}


def find_all(mm: mmap.mmap, needle: bytes, limit: int = 100) -> list[int]:
    hits: list[int] = []
    start = 0
    while len(hits) < limit:
        pos = mm.find(needle, start)
        if pos < 0:
            break
        hits.append(pos)
        start = pos + 1
    return hits


def ext4_candidates(mm: mmap.mmap) -> list[tuple[int, int, int, int]]:
    # ext2/3/4 superblock is 1024 bytes from the filesystem start; magic is +0x38.
    result: list[tuple[int, int, int, int]] = []
    pos = 0
    needle = b"\x53\xef"
    while len(result) < 100:
        pos = mm.find(needle, pos)
        if pos < 0:
            break
        fs_start = pos - 1024 - 0x38
        if fs_start >= 0 and fs_start % 512 == 0:
            sb = fs_start + 1024
            if sb + 0x68 <= len(mm):
                log_block = struct.unpack_from("<I", mm, sb + 0x18)[0]
                blocks = struct.unpack_from("<I", mm, sb + 0x04)[0]
                inodes = struct.unpack_from("<I", mm, sb + 0x00)[0]
                if log_block <= 6 and blocks and inodes:
                    result.append((fs_start, 1024 << log_block, blocks, inodes))
        pos += 1
    return result


def main(path: Path) -> None:
    print(f"file={path.name} size={path.stat().st_size}")
    with path.open("rb") as fh, mmap.mmap(fh.fileno(), 0, access=mmap.ACCESS_READ) as mm:
        print(f"sha256={hashlib.sha256(mm).hexdigest()}")
        for magic, label in MAGICS.items():
            hits = find_all(mm, magic)
            if hits:
                print(f"{label}: " + ", ".join(f"0x{x:x}" for x in hits))
        for fs_start, block_size, blocks, inodes in ext4_candidates(mm):
            size = block_size * blocks
            print(
                f"ext filesystem: start=0x{fs_start:x} block={block_size} "
                f"blocks={blocks} approx_size={size} inodes={inodes}"
            )

        patterns = [
            rb"Linux version [^\x00\r\n]{1,200}",
            rb"PRETTY_NAME=[^\x00\r\n]{1,200}",
            rb"root=[^\x00\r\n ]{1,200}",
            rb"/sbin/init",
            rb"systemd",
            rb"buildroot",
            rb"busybox",
            rb"wayland",
            rb"alsa",
            rb"mpc[-_ ]?sample",
        ]
        seen: set[bytes] = set()
        print("selected_strings:")
        for pattern in patterns:
            for match in re.finditer(pattern, mm, re.IGNORECASE):
                value = match.group()[:300]
                if value not in seen:
                    seen.add(value)
                    print(f"  0x{match.start():x}: {value.decode('ascii', 'replace')}")
                    if len(seen) >= 100:
                        return


if __name__ == "__main__":
    main(Path(sys.argv[1]))

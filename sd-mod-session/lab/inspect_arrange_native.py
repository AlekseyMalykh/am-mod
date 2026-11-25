"""Read-only address discovery in the pinned Sample ELF; never process memory.

RTTI names and direct calls are leads, not proof that a playback path is used.
Virtual addresses are translated through PT_LOAD (not treated as file offsets).
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

SHA = 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'


class Image:
    def __init__(self, path):
        self.data = Path(path).read_bytes()
        if hashlib.sha256(self.data).hexdigest() != SHA:
            raise ValueError('Unsupported native ELF')
        phoff, = struct.unpack_from('<Q', self.data, 32)
        size, count = struct.unpack_from('<HH', self.data, 54)
        self.loads = []
        for index in range(count):
            p = struct.unpack_from('<IIQQQQQQ', self.data, phoff + index * size)
            if p[0] == 1:
                self.loads.append((p[3], p[2], p[5], p[1]))

    def read(self, address, size):
        for va, offset, length, _ in self.loads:
            if va <= address and address + size <= va + length:
                return self.data[offset + address - va:offset + address - va + size]
        raise ValueError('Unmapped file-backed address: ' + hex(address))

    def q(self, address):
        return struct.unpack('<Q', self.read(address, 8))[0]

    def words(self, executable=False):
        for va, offset, length, flags in self.loads:
            if executable and not flags & 1:
                continue
            step = 4 if executable else 8
            fmt = '<I' if executable else '<Q'
            for delta in range(0, length - step + 1, step):
                yield va + delta, struct.unpack_from(fmt, self.data, offset + delta)[0]

    def types(self, name):
        encoded = name.encode() + b'\0'
        names = set()
        for va, offset, length, _ in self.loads:
            cursor = offset
            while True:
                hit = self.data.find(encoded, cursor, offset + length)
                if hit < 0:
                    break
                names.add(va + hit - offset)
                cursor = hit + 1
        infos = {address - 8 for address, value in self.words() if value in names}
        for address, value in self.words():
            if value in infos:
                # A primary vtable header has zero offset-to-top.
                if self.q(address - 8) == 0:
                    yield {'typeinfo': hex(value), 'vtable': hex(address + 8),
                           'slots': [hex(self.q(address + 8 + i * 8)) for i in range(12)]}

    def callers(self, target):
        entry = None
        for address, op in self.words(executable=True):
            if op == 0xd503233f:  # paciasp; approximate function boundary only
                entry = address
            if op & 0xfc000000 != 0x94000000:
                continue
            immediate = op & 0x3ffffff
            if immediate & 0x2000000:
                immediate -= 0x4000000
            if address + immediate * 4 == target:
                yield {'call': hex(address), 'preceding_paciasp': hex(entry) if entry else None}

    def references(self, target):
        pages = {}
        for address, op in self.words(executable=True):
            if op & 0x9f000000 == 0x90000000:  # ADRP
                immediate = ((op >> 5) & 0x7ffff) * 4 + ((op >> 29) & 3)
                if immediate & 0x100000:
                    immediate -= 0x200000
                pages[op & 31] = (address, (address & ~4095) + immediate * 4096)
                continue
            source = (op >> 5) & 31
            if source not in pages:
                continue
            start, page = pages[source]
            if address - start > 24:
                continue
            value, kind = None, None
            if op & 0xff800000 == 0x91000000:  # ADD Xd, Xn, #imm
                value = page + ((op >> 10) & 4095) * (4096 if op & (1 << 22) else 1)
                kind = 'adrp_add'
            elif op & 0xffc00000 == 0xf9400000:  # LDR Xd, [Xn, #imm]
                try:
                    value = self.q(page + ((op >> 10) & 4095) * 8)
                except ValueError:
                    pass
                kind = 'adrp_ldr'
            if value == target:
                yield {'reference': hex(address), 'page_instruction': hex(start), 'kind': kind}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock', required=True)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--type', help='Exact mangled RTTI name, e.g. 9Sequencer')
    group.add_argument('--callers', type=lambda value: int(value, 0))
    group.add_argument('--references', type=lambda value: int(value, 0),
                       help='ADRP + ADD/LDR within six instructions; candidates only')
    args = parser.parse_args()
    image = Image(args.stock)
    values = (image.types(args.type) if args.type else image.callers(args.callers)
              if args.callers is not None else image.references(args.references))
    print(json.dumps(list(values), indent=2))


if __name__ == '__main__':
    main()

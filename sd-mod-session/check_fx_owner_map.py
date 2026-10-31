"""Pin the application/FX owner ABI to instructions in stock1.3.0.12."""
from pathlib import Path
from hashlib import sha256
import struct

root = Path(__file__).resolve().parent
image = (root.parent / 'extracted_v1.3.0/partitions/part3/usr/bin/MPC Sample').read_bytes()
assert sha256(image).hexdigest() == 'b6584cba76e416e432ad202509099d34373dc2c3d6a8fb625f5678bf0193b508'
# Each word below is from the native constructor, not the mod's fixture.
# Text VA == file offset in this pinned ELF. RELRO VA == file offset+10000.
instructions = {
    0xfeec34: 0xb0030fa2,    # adrp x2,71e3000 (JUCE instance storage)
    0xfeec44: 0xf906e440,    # str x0,[x2,3528] ->71e3dc8
    0xfeec7c: 0xf906e45f,    # destructor clears that same instance
    0x14379f0: 0x9002ce80,  # app vptr page6e07000
    0x14379f4: 0x910a2000,  # app vptr6e07288
    0x14379f8: 0xf9000260,  # store app vptr
    0x1437a38: 0x9402cb8a,  # construct AudioIO14ea860
    0x1437a40: 0xf9006e74,  # app+d8 = AudioIO
    0x1437a70: 0xf9007e60,  # app+f8 = UI registry
    0x14ea954: 0xd002c900,  # AudioIO vptr page6e0c000
    0x14ea958: 0x9110c000,  # AudioIO vptr6e0c430
    0x14ea95c: 0xf9000280,  # store AudioIO vptr
    0x14ea9e0: 0x94000e34,  # construct core14ee2b0
    0x14ea9e8: 0xf900a69b,  # AudioIO+148 = core
    0x14ee430: 0xd002c8e3,  # core vptr page6e0c000
    0x14ee434: 0x913ce063,  # core vptr6e0cf38
    0x14ee448: 0xf9000383,  # store core vptr
    0x14ee524: 0x9002c4e1,  # playhead vptr page6d8a000
    0x14ee528: 0x91106021,  # playhead vptr6d8a418
    0x14ee538: 0xf9024b93,  # core+490 = playhead
    0x14ee6e8: 0x942c14f2,  # construct PluginManager1ff3ab0
    0x14ee6ec: 0xf9026b93,  # core+4d0 = manager
}
for address, expected in instructions.items():
    actual = struct.unpack_from('<I', image, address)[0]
    assert actual == expected, f'Native ABI changed at {address:x}: {actual:08x}'
assert struct.unpack_from('<Q', image, 0x6d8a428 - 0x10000)[0] == 0x13ad070
print(f'PASS native FX owner map: {len(instructions)} pinned constructor/destructor instructions and real playhead getter')

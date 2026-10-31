from pathlib import Path


path = Path("/home/user/mpc-rootfs-user-20260808/usr/bin/test-app-launcher")
data = bytearray(path.read_bytes())
offset = 0x33D120
expected = bytes.fromhex("20050036")
replacement = (0xD503201F).to_bytes(4, "little")

actual = bytes(data[offset : offset + 4])
print(f"offset=0x{offset:x} before={actual.hex()}")
if actual != expected:
    raise SystemExit(f"unexpected instruction: {actual.hex()}")

data[offset : offset + 4] = replacement
path.write_bytes(data)
print(f"offset=0x{offset:x} after={replacement.hex()}")

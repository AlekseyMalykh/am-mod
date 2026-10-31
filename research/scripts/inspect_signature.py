from __future__ import annotations

import sys
import hashlib
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa


def main(image_path: Path, key_path: Path) -> None:
    image = image_path.read_bytes()
    key = serialization.load_pem_public_key(key_path.read_bytes())
    if not isinstance(key, rsa.RSAPublicKey):
        raise SystemExit("Not an RSA public key")
    numbers = key.public_numbers()
    width = (numbers.n.bit_length() + 7) // 8
    print(f"rsa_bits={numbers.n.bit_length()} exponent={numbers.e} signature_bytes={width}")

    found = False
    for offset in range(0x188, 0x1A1):
        raw = image[offset : offset + width]
        if len(raw) != width:
            continue
        for byte_order in ("big", "little"):
            value = int.from_bytes(raw, byte_order)
            if value >= numbers.n:
                continue
            encoded = pow(value, numbers.e, numbers.n).to_bytes(width, "big")
            if encoded.startswith(b"\x00\x01\xff"):
                found = True
                separator = encoded.find(b"\x00", 2)
                digest_info = encoded[separator + 1 :] if separator >= 0 else b""
                print(
                    f"pkcs1_v1_5_signature offset=0x{offset:x} byte_order={byte_order} "
                    f"padding_end=0x{separator:x}"
                )
                print(f"digest_info={digest_info.hex()}")
    if not found:
        print("No PKCS#1 v1.5 signature found in candidate header range")

    expected = bytes.fromhex("84d347ce3954cd71d1dcd544ec6e97e5bfdece38af4a130a3571726cab2f3778")
    candidates = {
        "header_to_0x190": image[:0x190],
        "header_to_signature": image[:0x198],
        "payload_after_signature": image[0x298:],
        "file_without_signature": image[:0x198] + image[0x298:],
        "entries_0x90_to_0x190": image[0x90:0x190],
        "entries_0x90_to_signature": image[0x90:0x198],
        "file_with_zero_signature": image[:0x198] + bytes(width) + image[0x298:],
    }
    print("sha256_candidates:")
    for name, candidate in candidates.items():
        digest = hashlib.sha256(candidate).digest()
        print(f"  {name}={digest.hex()} match={digest == expected}")


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))

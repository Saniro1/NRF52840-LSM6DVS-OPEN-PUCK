"""Install the radio sender into one verified Fix42 image."""

import argparse
import hashlib
from pathlib import Path
import struct
import subprocess

from uf2 import Fix42Image

PROFILES = {
    "normal": "d09a01816048f5a84cdda909d03fec308e2184dbdedcba400b1055aec664bb79",
    "inverted": "a29b8a0ed1b154d6d51425d8261a2b5a632471d69dbdd705d9581bdf42d8db68",
    "handheld": "36906cdb55bee571a509c97bb1a0711694115642ea8d68debe743365311838ba",
}
PATCH_START = 0x3B000
PATCH_SIZE = 0x1000
ESB_THREAD_ENTRY = 0x30244
CLOCK_STOP = 0x8A6C
MOTION_WRITER = 0x99B8


def branch_thumb(source, target):
    displacement = target - (source + 4)
    if displacement & 1 or not -(1 << 24) <= displacement < (1 << 24):
        raise ValueError("Unencodable Thumb branch displacement")
    offset = displacement & 0x1FFFFFF
    s = (offset >> 24) & 1
    j1 = ((offset >> 23) & 1) ^ s ^ 1
    j2 = ((offset >> 22) & 1) ^ s ^ 1
    return struct.pack("<HH", 0xF000 | (s << 10) | ((offset >> 12) & 0x3FF),
                       0x9000 | (j1 << 13) | (j2 << 11) | ((offset >> 1) & 0x7FF))


def load_symbols(nm, elf):
    output = subprocess.check_output([nm, "--defined-only", str(elf)], text=True)
    return {name: int(address, 16) for address, _, name in
            (line.split() for line in output.splitlines())}


def apply(profile, original, code, symbols):
    if hashlib.sha256(original).hexdigest() != PROFILES[profile]:
        raise ValueError(f"Unsupported Fix42 {profile} image. SHA-256 did not match.")
    transmit, publish = (symbols[name] for name in ("puck_transmit", "puck_publish"))
    if transmit != PATCH_START or not PATCH_START < publish < PATCH_START + len(code) <= PATCH_START + PATCH_SIZE:
        raise ValueError("Unrecognized transmitter link layout")
    updated = Fix42Image(original)
    if updated.read(PATCH_START, PATCH_SIZE) != b"\xff" * PATCH_SIZE:
        raise ValueError("Expected erased Fix42 application flash page")
    updated.patch(PATCH_START, b"\xff" * len(code), code)
    updated.patch(ESB_THREAD_ENTRY, struct.pack("<I", 0x9195), struct.pack("<I", transmit | 1))
    # Give the radio thread exclusive ownership of the HF crystal request.
    updated.patch(CLOCK_STOP, bytes.fromhex("044b"), bytes.fromhex("7047"))  # bx lr
    updated.patch(MOTION_WRITER, bytes.fromhex("a3f022b9"), branch_thumb(MOTION_WRITER, publish))
    return updated.data


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", default="arm-none-eabi-nm")
    parser.add_argument("profile", choices=PROFILES)
    for path in ("base", "elf", "code", "output"):
        parser.add_argument(path, type=Path)
    args = parser.parse_args()
    image = apply(args.profile, args.base.read_bytes(), args.code.read_bytes(),
                  load_symbols(args.nm, args.elf))
    args.output.write_bytes(image)

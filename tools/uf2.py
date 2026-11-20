"""Build application-only UF2 and read the exact Fix42 flash layout."""

import argparse
from pathlib import Path
import struct

BLOCK_SIZE = 512
PAYLOAD_SIZE = 256
MAGIC = (0x0A324655, 0x9E5D5157)
END_MAGIC = 0x0AB16F30
UF2_FAMILY_FLAG = 0x2000
APPLICATION_START = 0x1000
APPLICATION_END = 0xEA000
FIX42_FAMILY = 0x239A00B3


def encode(image, address, family):
    count = (len(image) + PAYLOAD_SIZE - 1) // PAYLOAD_SIZE
    result = bytearray()
    for number in range(count):
        offset = number * PAYLOAD_SIZE
        header = struct.pack(
            "<8I", *MAGIC, UF2_FAMILY_FLAG, address + offset, PAYLOAD_SIZE,
            number, count, family,
        )
        payload = image[offset:offset + PAYLOAD_SIZE].ljust(PAYLOAD_SIZE, b"\xff")
        result.extend((header + payload).ljust(BLOCK_SIZE - 4, b"\0"))
        result.extend(struct.pack("<I", END_MAGIC))
    return result


class Fix42Image:
    def __init__(self, data):
        self.data = bytearray(data)
        self.blocks = {}
        count = (APPLICATION_END - APPLICATION_START) // PAYLOAD_SIZE
        if len(data) != count * BLOCK_SIZE:
            raise ValueError("Unexpected Fix42 UF2 size")

        numbers = set()
        for offset in range(0, len(data), BLOCK_SIZE):
            block = data[offset:offset + BLOCK_SIZE]
            m0, m1, flags, address, size, number, total, family = struct.unpack_from("<8I", block)
            end, = struct.unpack_from("<I", block, BLOCK_SIZE - 4)
            if (m0, m1) != MAGIC or end != END_MAGIC or flags != UF2_FAMILY_FLAG:
                raise ValueError("Invalid UF2 block header")
            if size != PAYLOAD_SIZE or total != count or number >= count or number in numbers:
                raise ValueError("Invalid UF2 payload or numbering")
            if family != FIX42_FAMILY or address in self.blocks or address % size:
                raise ValueError("Invalid Fix42 family or duplicate/misaligned address")
            if not APPLICATION_START <= address <= APPLICATION_END - size:
                raise ValueError("Invalid Fix42 flash address")
            numbers.add(number)
            self.blocks[address] = offset + 32

    def offsets(self, address, size):
        for byte_address in range(address, address + size):
            page, within_page = divmod(byte_address, PAYLOAD_SIZE)
            yield self.blocks[page * PAYLOAD_SIZE] + within_page

    def read(self, address, size):
        return bytes(self.data[offset] for offset in self.offsets(address, size))

    def patch(self, address, old, new):
        if len(old) != len(new) or self.read(address, len(old)) != old:
            raise ValueError(f"Flash patch precondition failed at {address:#x}")
        for offset, byte in zip(self.offsets(address, len(new)), new):
            self.data[offset] = byte


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("address", type=lambda x: int(x, 0))
    parser.add_argument("family", type=lambda x: int(x, 0))
    args = parser.parse_args()
    args.output.write_bytes(encode(args.source.read_bytes(), args.address, args.family))

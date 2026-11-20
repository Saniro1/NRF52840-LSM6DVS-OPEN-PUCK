"""Check flash boundaries and execute the linked controller patch on ARM."""

from pathlib import Path
import struct
import sys

from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn import UC_MODE_MCLASS, UC_MODE_THUMB
from unicorn.arm_const import (
    UC_ARM_REG_C1_C0_2, UC_ARM_REG_FPEXC, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_PRIMASK,
    UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
)

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from patch_fix42 import CLOCK_STOP, ESB_THREAD_ENTRY, MOTION_WRITER, PATCH_START
from patch_fix42 import load_symbols
from uf2 import Fix42Image, encode

BUILD, DEPS, NM = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
RADIO = 0x40001000
GENERATION = 0x20003274
ACCEL = 0x20005106
GYRO = 0x2000510C
STACK_TOP = 0x20005A80
STOP = 0x1200


def read_u32(memory, address):
    return struct.unpack("<I", memory.mem_read(address, 4))[0]


def receiver_images():
    variants = {
        "mbr-0x1000": (0x1000, 0xADA52840),
        "slime-pro-micro": (0x1000, 0x239A00B3),
        "s140-v6-0x26000": (0x26000, 0xADA52840),
        "s140-v7-0x27000": (0x27000, 0xADA52840),
    }
    for name, (address, family) in variants.items():
        stem = BUILD / f"receiver-{address:#x}"
        program = stem.with_suffix(".bin").read_bytes()
        symbols = load_symbols(NM, stem.with_suffix(".elf"))
        stack, entry = struct.unpack_from("<II", program)
        assert 0x20003400 < stack <= 0x20040000
        assert entry == symbols["Reset_Handler"] | 1
        assert address <= entry < address + len(program) < 0xED000
        assert struct.unpack_from("<I", program, 15 * 4)[0] == symbols["SysTick_Handler"] | 1
        assert struct.unpack_from("<I", program, (16 + 39) * 4)[0] == symbols["USBD_IRQHandler"] | 1
        assert (BUILD / f"receiver-{name}.uf2").read_bytes() == encode(program, address, family)
    print("Receiver vector tables and UF2 flash layouts passed")


def controller(profile, mapping):
    stem = BUILD / f"transmitter-fix42-{profile}"
    old = Fix42Image((DEPS / "fix42" / f"fix42-ds4-{profile}.uf2").read_bytes())
    new = Fix42Image(stem.with_suffix(".uf2").read_bytes())
    symbols = load_symbols(NM, stem.with_suffix(".elf"))
    patch_size = stem.with_suffix(".bin").stat().st_size

    changes = set()
    for address, length in ((ESB_THREAD_ENTRY, 4), (CLOCK_STOP, 2),
                            (MOTION_WRITER, 4), (PATCH_START, patch_size)):
        changes.update(new.offsets(address, length))
    assert all(a == b or i in changes for i, (a, b) in enumerate(zip(old.data, new.data)))
    entry, = struct.unpack("<I", new.read(ESB_THREAD_ENTRY, 4))
    assert entry == symbols["puck_transmit"] | 1

    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
    cpu.mem_map(0, 0x100000)
    cpu.mem_map(0x20000000, 0x40000)
    cpu.mem_map(RADIO, 0x1000)
    cpu.mem_write(0x1000, new.read(0x1000, 0x3C000))
    cpu.mem_write(0xACC00, new.read(0xACC00, 0x300))
    cpu.reg_write(UC_ARM_REG_C1_C0_2, 0xF << 20)
    cpu.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
    cpu.reg_write(UC_ARM_REG_SP, STACK_TOP)

    # Exercise the patched entry with the original Fix42 converter first.
    for mask, gyro_valid, expected in ((1, 0, 0), (0, 1, 1)):
        cpu.reg_write(UC_ARM_REG_LR, STOP | 1)
        cpu.reg_write(UC_ARM_REG_PRIMASK, mask)
        cpu.reg_write(UC_ARM_REG_R0, gyro_valid)
        cpu.reg_write(UC_ARM_REG_R1, 0x20010000)
        cpu.reg_write(UC_ARM_REG_R2, 1)
        cpu.reg_write(UC_ARM_REG_R3, 0x20010010)
        cpu.mem_write(0x20010000, struct.pack("<3f", 1.1, -2.1, 3.1))
        cpu.mem_write(0x20010010, struct.pack("<3f", 0.5, -1, 0.25))
        try:
            cpu.emu_start(MOTION_WRITER | 1, STOP, count=2000)
        except Exception:
            print(f"writer stopped at {cpu.reg_read(UC_ARM_REG_PC):#x}")
            raise
        assert cpu.reg_read(UC_ARM_REG_PC) == STOP
        assert cpu.reg_read(UC_ARM_REG_PRIMASK) == mask
        assert cpu.reg_read(UC_ARM_REG_SP) == STACK_TOP
        assert read_u32(cpu, GENERATION) == expected
        assert struct.unpack("<3h", cpu.mem_read(ACCEL, 6)) == (4096, -8192, 2048)

    # A mock Zephyr clock and RADIO completion drive the actual compiled loop.
    ticks = 0xFFFFFF00
    origin = ticks
    tx_ticks, packets = [], []
    initial_generation = 0
    clock_requests = 0
    disabled_recovery = 0
    rx_values = (-32768, -32, 48, 4096, -32768, 2048)
    cpu.mem_write(GENERATION, struct.pack("<I", initial_generation))
    cpu.mem_write(GYRO, struct.pack("<3h", *rx_values[:3]))
    cpu.mem_write(ACCEL, struct.pack("<3h", *rx_values[3:]))

    def kernel(uc, address, size, user_data):
        nonlocal ticks, clock_requests
        if address == 0x8B9C:
            clock_requests += 1
            uc.reg_write(UC_ARM_REG_R0, 0)
        elif address == 0x2FAF8:
            uc.reg_write(UC_ARM_REG_R0, ticks & 0xFFFFFFFF)
            uc.reg_write(UC_ARM_REG_R1, ticks >> 32)
        elif address == 0x2A20C:
            wait = uc.reg_read(UC_ARM_REG_R0)
            assert 0 < wait <= 33 and uc.reg_read(UC_ARM_REG_R1) == 0
            ticks += wait + 1
            elapsed = ticks - origin
            if 65 <= elapsed < 32833 or 35000 <= elapsed:
                uc.mem_write(GENERATION, struct.pack("<I", elapsed))
            if elapsed > 36000:
                uc.emu_stop()
        else:
            return
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    def radio(uc, access, address, size, value, user_data):
        nonlocal disabled_recovery
        if address == RADIO + 0x010:  # TASKS_DISABLE
            disabled_recovery += 1
            uc.mem_write(RADIO + 0x550, b"\0" * 4)
        if address == RADIO:  # TASKS_TXEN
            assert uc.reg_read(UC_ARM_REG_PRIMASK) == 0
            pointer = read_u32(uc, RADIO + 0x504)
            assert (pointer & 3) == 0 and STACK_TOP - 512 <= pointer < STACK_TOP
            tx_ticks.append(ticks - origin)
            packets.append(struct.unpack("<HH6h", uc.mem_read(pointer, 16)))
            if len(packets) == 40:
                # Keep one transfer busy beyond the following deadline.
                uc.mem_write(RADIO + 0x550, struct.pack("<I", 11))

    cpu.hook_add(UC_HOOK_CODE, kernel)
    cpu.hook_add(UC_HOOK_MEM_WRITE, radio, begin=RADIO, end=RADIO + 0xFFF)
    cpu.emu_start(entry, STOP, count=1000000)

    def map_axes(values):
        return tuple(min(32767, values[axis - 1] if axis > 0 else -values[-axis - 1])
                     for axis in mapping)

    expected_axes = map_axes(rx_values[:3]) + map_axes(rx_values[3:])
    assert clock_requests == 1 and disabled_recovery == 1
    assert read_u32(cpu, RADIO + 0x510) == 1  # NRF_2Mbit mode
    assert read_u32(cpu, RADIO + 0x518) == 0x02031010  # 16 bytes, four-byte address, whitening
    assert tx_ticks[0] >= 65 and 995 <= sum(t < 32833 for t in tx_ticks) <= 1000
    assert not any(34500 < t < 35000 for t in tx_ticks)
    assert tx_ticks[-1] > 35000
    for sequence, packet in enumerate(packets, 1):
        assert packet == (0x5041, sequence, *expected_axes)
    print(f"{profile}: original writer, mapped ARM radio packets, cadence, timeout and recovery passed")


receiver_images()
for profile, mapping in {"normal": (2, -3, -1), "inverted": (-2, -3, 1),
                         "handheld": (2, 1, -3)}.items():
    controller(profile, mapping)

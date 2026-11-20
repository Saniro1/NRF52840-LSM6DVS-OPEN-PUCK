# Build and verify Open Puck

The receiver uses TinyUSB and the Nordic nRF52840 radio and USB peripherals.
It uses the board's 32 MHz crystal and USB connection. Puck status is visible
through USB enumeration and motion in the host app. The firmware requires
no receiver IMU, UART, or external GPIO wiring.

## Build from source

On Linux, install GNU Make, a host C compiler, GNU Arm Embedded GCC with Newlib
(`arm-none-eabi-gcc`), Python 3, curl, and standard archive tools. The release
uses GNU Arm Embedded 9-2019-q4-major. The dependency fetch checks the hashes
of TinyUSB, nrfx, CMSIS, and each controller image.

```sh
make fetch
make -j
```

Build outputs are in `build/`. To use a toolchain outside your `PATH`,
pass `CROSS=/path/to/bin/arm-none-eabi-` to Make.

To run the host and ARM checks, install the pinned Unicorn package into a
Python virtual environment and pass that Python to Make.

```sh
python3 -m venv .deps/venv
.deps/venv/bin/python -m pip install unicorn==2.1.4
make check PYTHON=.deps/venv/bin/python
```

The tests validate receiver vectors and UF2 address ranges, DS4 input and
feature report bytes, loss and reacquisition, and the exact controller patch
locations. The ARM test runs the original Fix42 motion writer and the new
transmitter code with modeled Zephyr time and radio completion. The simulated
cadence describes scheduling logic using a virtual clock.

## Wireless protocol

The defaults are Nordic proprietary 2 Mbit/s at 2476 MHz, 0 dBm TX power,
address prefix `0xe3` and base `0xb7519d00`. The address uses the upper three
bytes of the base. Both radios share the same fixed 16-byte payload layout,
whitening, and CRC-16. The stream is one-way and unencrypted, with no
acknowledgments or retransmits. The receiver keeps the newest valid packet
and uses a 30 ms gyro timeout. It retains the last known acceleration to
avoid an abrupt change in the host's gravity reference.

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 2 | Protocol magic `0x5041` |
| 2 | 2 | Transmitter sequence, wraps at 65536 |
| 4 | 6 | Gyro X, Y, Z as signed 16-bit values at 16 counts per °/s |
| 10 | 6 | Accel X, Y, Z as signed 16-bit values at 8192 counts per g |

Fields are little-endian. Channel and address can be varied by rebuilding
both ends with the same `PUCK_CHANNEL` and `PUCK_ADDRESS_BASE` Make variables.
The nRF channel indexes MHz above 2400. Choose an address per link if you
operate multiple transmitters at once.

## Receiver USB

The puck advertises USB HID `054c:05c4` for DS4 parser compatibility. The input
report is ID 1, exactly 64 bytes, with a 1 ms interrupt-IN interval. Motion
starts at byte 13 for gyro and byte 19 for acceleration. The other fields
include a wrapping 6-bit counter, receiver-clock timestamp, neutral buttons
and sticks, and inactive touch contacts. This is a gyro accessory. Buttons,
rumble, and other controller functions stay with your gamepad.

The receiver also replies to the USB feature reports used by Linux's
DualShock 4 probe with a device-derived identifier, nominal motion scales,
and firmware version. Initial compatibility target is the wired DS4 packet
parser used by JoyShockLibrary and JoyShockMapper Legacy.
[Linux DS4 probe and format](https://github.com/torvalds/linux/blob/v6.15/drivers/hid/hid-playstation.c)

To inspect raw USB arrival rate and motion on the connected PC, use the
controller repository's `tools/measure_ds4.py` with `hidapi==0.14.0.post4`.
Select the puck's index after listing devices.

```sh
python tools/measure_ds4.py --list
python tools/measure_ds4.py --device INDEX --seconds 30 --csv puck.csv
```

This counts HID report arrivals. Repeated motion values can come from a still
controller, Fix42 FIFO publication timing, or reuse of the latest radio packet.
A USB bus trace measures bus timing. RF and sensor throughput require separate
measurement.
[HID measurement tool](https://github.com/Saniro1/nRF52840-LSM6DVS-1000Hz-Firmware-Test/blob/6c3f1a707a19950a8a5af6eb34e7db7001d0f096/tools/measure_ds4.py)

## Controller integration

The transmitter base is commit `6c3f1a707a19950a8a5af6eb34e7db7001d0f096` of
`ds4-fix42-usb-1000hz`. Fix42's working release is a patched Zephyr/SlimeVR UF2.
The build checks the exact image hash and verifies the thread entry, motion
writer and clock release bytes before applying the update. The new code lives
in an erased page of the application flash.

The Open Puck sender replaces the SlimeVR ESB thread and holds the Zephyr HF
crystal request while running. This can increase battery use. It reads the
same calibrated motion snapshot as wired Fix42, uses the same axis mapping,
and stops transmitting when no valid gyro publication has occurred for 50 ms.
The USB HID report loop and USB shell are retained. Flash the corresponding
original Fix42 UF2 to return to the wired build.
[Fix42 source and image manifest](https://github.com/Saniro1/nRF52840-LSM6DVS-1000Hz-Firmware-Test/tree/6c3f1a707a19950a8a5af6eb34e7db7001d0f096/releases/ds4-fix42)

## Third-party notices

The receiver links pinned TinyUSB 0.18.0, Nordic nrfx and Arm CMSIS code.
The controller patch starts from Saniro1's Fix42 binaries based on SlimeVR
Tracker nRF. Upstream notices are in [licenses/](licenses/).
[TinyUSB](https://github.com/hathach/tinyusb/tree/0.18.0),
[nrfx](https://github.com/NordicSemiconductor/nrfx/tree/7c47cc0a56ce44658e6da2458e86cd8783ccc4a2),
[CMSIS](https://github.com/ARM-software/CMSIS_5/tree/2b7495b8535bdcb306dac29b9ded4cfb679d7e5c),
[SlimeVR Tracker nRF](https://github.com/SlimeVR/SlimeVR-Tracker-nRF/tree/cbfff16ebed1659d4c0e9f361ded0030fb66855a)

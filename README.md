# Open Puck wireless gyro

An nRF52840 and LSM6DSV attached to your controller send three-axis gyro and
accelerometer readings to a second nRF52840 over 2.4 GHz. The receiver plugs into
your PC and presents a USB DualShock 4-compatible motion HID device for the
JoyShockMapper Legacy/JoyShockLibrary path. Connect the actual gamepad to your
PC as usual for its buttons and sticks.

**Download the prebuilt receiver and controller firmware.**
[Open Puck v0.1.0 experimental release](https://github.com/Saniro1/NRF52840-LSM6DVS-OPEN-PUCK/releases/tag/v0.1.0)

## 1. Identify and flash the USB receiver

If your receiver has a UF2 bootloader, connect it with a USB data cable, enter
bootloader mode (often by double-tapping Reset), and open `INFO_UF2.TXT` on its
drive. Use the model, board ID, and bootloader documentation to match the
application layout in this table. Copy the matching UF2 to the bootloader
drive. The drive normally disappears as the receiver restarts.

| Confirmed receiver bootloader | Release file |
| --- | --- |
| SlimeNRF ProMicro bootloader, Board-ID `nRF52840-SlimeNRFTracker-ProMicro` | `receiver-slime-pro-micro.uf2` |
| Standard nRF52840 UF2, MBR-only layout with application at `0x1000` | `receiver-mbr-0x1000.uf2` |
| Standard nRF52840 UF2 with S140 v6.x and application at `0x26000` | `receiver-s140-v6-0x26000.uf2` |
| Standard nRF52840 UF2 with S140 v7.x and application at `0x27000` | `receiver-s140-v7-0x27000.uf2` |
| Nordic nRF52840 USB Dongle (PCA10059), stock Nordic DFU | `receiver-pca10059.hex`, packaged as shown below |

Confirm the layout before flashing. SuperMini and nice!nano-compatible boards
can ship with different bootloaders. The chip name alone does not determine
the layout. Keep the original receiver image or vendor recovery instructions.
[Adafruit UF2 bootloader and application addresses](https://github.com/adafruit/Adafruit_nRF52_Bootloader#making-your-own-uf2)

### Nordic USB Dongle flashing

For the PCA10059 with its stock Nordic bootloader, install Nordic `nrfutil` and
run `nrfutil install nrf5sdk-tools`. Press Reset on the dongle, verify the red
bootloader LED pulses, and use the bootloader's serial port in the last command.
The example port is for Linux. On Windows it will be a `COM` port.

```sh
nrfutil nrf5sdk-tools pkg generate --hw-version 52 --sd-req=0x00 \
  --application receiver-pca10059.hex --application-version 1 receiver-pca10059.zip
nrfutil nrf5sdk-tools dfu usb-serial -pkg receiver-pca10059.zip -p /dev/ttyACM0
```

[Nordic dongle DFU steps](https://docs.zephyrproject.org/latest/boards/nordic/nrf52840dongle/doc/index.html#option-1-using-the-built-in-bootloader-only)

## 2. Flash the controller-mounted gyro

The stock Fix42 images need the Open Puck transmitter update. Choose the
same mounting you already use and flash its `transmitter-fix42-*.uf2` through the
controller board's established double-reset UF2 procedure.

| Your Fix42 mounting | Release file |
| --- | --- |
| Normal, USB-C up | `transmitter-fix42-normal.uf2` |
| Inverted, USB-C down | `transmitter-fix42-inverted.uf2` |
| Handheld | `transmitter-fix42-handheld.uf2` |

Keep your working wired UF2 so you can restore it if needed. The transmitter
uses the Fix42 sensor calibration, VQF correction, deadzone, and axis mapping.
The existing USB shell is available with a data cable if you need to review
settings. Use your existing Fix42 settings and warm-up routine.
[Original Fix42 images and setup](https://github.com/Saniro1/nRF52840-LSM6DVS-1000Hz-Firmware-Test/blob/6c3f1a707a19950a8a5af6eb34e7db7001d0f096/docs/DS4_FIX42.md)

## 3. Check motion on the PC

1. Plug the puck into the PC with a USB data cable. Power the gyro board from
   its usual battery or USB power source. During wireless testing, use battery
   or a power-only connection to avoid listing the wired gyro as a second HID
   device on the PC.
2. Launch JoyShockMapper with Legacy/JoyShockLibrary and select the puck. Its
   USB product string is `Open Puck Wireless Gyro`. Start with `GYRO_SPACE = LOCAL`.
   After warm-up, run host gyro calibration with the controller resting still.
3. Move the controller about each axis. If a direction is reversed, confirm
   you flashed the matching mounting profile. Turn off the transmitter and
   verify gyro motion stops. Turn it back on and verify that motion resumes.

The two release images share a radio channel and address, so they connect
automatically. Test one transmitter per radio address. If USB enumerates and
motion never arrives, check that the gyro board runs one of the
`transmitter-fix42-*.uf2` files and stays powered. The puck continues reporting
zero gyro after 30 ms without an accepted packet. When the controller sleeps,
check its existing Fix42 USB shell power settings. The Fix42 setup documents
`use_imu_timeout` and `use_active_timeout`.
[Fix42 power and sensor setup](https://github.com/Saniro1/nRF52840-LSM6DVS-1000Hz-Firmware-Test/blob/6c3f1a707a19950a8a5af6eb34e7db7001d0f096/docs/DS4_FIX35.md#configure-and-calibrate-the-physical-board)

**Validation.** All firmware variants cross-compiled. Host tests checked the
receiver reports and flash layouts. ARM emulation exercised the controller
patch on all three Fix42 profiles. USB enumeration, host application
recognition, RF range, latency, battery life, and sustained rates still need
a two-board test. The USB endpoint requests a 1 ms poll interval and the
transmitter targets 1,000 packets/s. Fix42's nominal 6 ms motion collection
can repeat data across several packets.

[Build instructions and protocol details](docs/firmware.md)

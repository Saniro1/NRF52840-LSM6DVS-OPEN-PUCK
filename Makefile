CROSS ?= arm-none-eabi-
HOST_CC ?= cc
PYTHON ?= python3
DEPS_DIR ?= .deps
BUILD ?= build
PUCK_CHANNEL ?= 76
PUCK_ADDRESS_BASE ?= 0xb7519d00u

NRFX := $(DEPS_DIR)/nrfx
CMSIS := $(DEPS_DIR)/cmsis/CMSIS/Core/Include
TINYUSB := $(DEPS_DIR)/tinyusb
TUSB_SRC := $(TINYUSB)/src
MDK := $(NRFX)/mdk

CPU := -mcpu=cortex-m4 -mthumb
RADIO_DEFINES := -DPUCK_CHANNEL=$(PUCK_CHANNEL) -DPUCK_ADDRESS_BASE=$(PUCK_ADDRESS_BASE)
BSP := -DNRF52840_XXAA -Icommon -isystem $(MDK) -isystem $(CMSIS) $(RADIO_DEFINES)
FLAGS := $(CPU) -Os -g -ffunction-sections -fdata-sections
APP_WARN := -Wall -Wextra -Werror
RX_INCLUDES := -Ireceiver -isystem $(TUSB_SRC) -isystem $(NRFX) -isystem $(NRFX)/hal \
    -isystem $(NRFX)/drivers/include -isystem $(NRFX)/drivers/src -isystem $(TINYUSB)/hw/bsp/nrf
RX_SRC := receiver/main.c receiver/ds4.c receiver/usb_descriptors.c
USB_SRC := $(addprefix $(TUSB_SRC)/,tusb.c common/tusb_fifo.c device/usbd.c \
    device/usbd_control.c class/hid/hid_device.c portable/nordic/nrf5x/dcd_nrf5x.c)
RX_OBJ := $(addprefix $(BUILD)/,$(notdir $(RX_SRC:.c=.o) $(USB_SRC:.c=.o))) \
    $(BUILD)/system_nrf52840.o $(BUILD)/gcc_startup_nrf52840.o
TX_PROFILES := normal inverted handheld
TX_UF2 := $(addprefix $(BUILD)/transmitter-fix42-,$(addsuffix .uf2,$(TX_PROFILES)))
RX_UF2 := $(addprefix $(BUILD)/receiver-,mbr-0x1000.uf2 slime-pro-micro.uf2 \
    s140-v6-0x26000.uf2 s140-v7-0x27000.uf2)
IMAGES := $(RX_UF2) $(BUILD)/receiver-pca10059.hex $(TX_UF2)

.PHONY: all fetch check
.SECONDARY:
all: $(IMAGES)

fetch:
	DEPS_DIR="$(DEPS_DIR)" bash tools/fetch-sources.sh

$(BUILD):
	mkdir -p $@

$(BUILD)/radio-settings: FORCE | $(BUILD)
	@printf '%s\n' '$(PUCK_CHANNEL) $(PUCK_ADDRESS_BASE)' > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

.PHONY: FORCE
FORCE:

$(BUILD)/%.o: receiver/%.c $(wildcard receiver/*.h common/*.h) $(BUILD)/radio-settings | $(BUILD)
	$(CROSS)gcc $(FLAGS) $(APP_WARN) $(BSP) $(RX_INCLUDES) -c $< -o $@

define usb_rule
$(BUILD)/$(notdir $(1:.c=.o)): $(1) receiver/tusb_config.h | $(BUILD)
	$(CROSS)gcc $(FLAGS) $(BSP) $(RX_INCLUDES) -c $$< -o $$@
endef
$(foreach src,$(USB_SRC) $(MDK)/system_nrf52840.c,$(eval $(call usb_rule,$(src))))

$(BUILD)/gcc_startup_nrf52840.o: $(MDK)/gcc_startup_nrf52840.S | $(BUILD)
	$(CROSS)gcc $(CPU) -D__START=main -D__HEAP_SIZE=0 -D__STACK_SIZE=2048 \
	    -D__STARTUP_CLEAR_BSS -c $< -o $@

$(BUILD)/receiver-%.elf: $(RX_OBJ) receiver/memory.ld
	$(CROSS)gcc $(CPU) -nostartfiles --specs=nano.specs --specs=nosys.specs \
	    -Wl,--gc-sections,--defsym=app_start=$*,--print-memory-usage \
	    -L$(MDK) -T receiver/memory.ld $(RX_OBJ) -o $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(CROSS)objcopy -O binary $< $@

$(BUILD)/receiver-mbr-0x1000.uf2: $(BUILD)/receiver-0x1000.bin tools/uf2.py
	$(PYTHON) tools/uf2.py $< $@ 0x1000 0xada52840

$(BUILD)/receiver-slime-pro-micro.uf2: $(BUILD)/receiver-0x1000.bin tools/uf2.py
	$(PYTHON) tools/uf2.py $< $@ 0x1000 0x239a00b3

$(BUILD)/receiver-s140-v6-0x26000.uf2: $(BUILD)/receiver-0x26000.bin tools/uf2.py
	$(PYTHON) tools/uf2.py $< $@ 0x26000 0xada52840

$(BUILD)/receiver-s140-v7-0x27000.uf2: $(BUILD)/receiver-0x27000.bin tools/uf2.py
	$(PYTHON) tools/uf2.py $< $@ 0x27000 0xada52840

$(BUILD)/receiver-pca10059.hex: $(BUILD)/receiver-0x1000.elf
	$(CROSS)objcopy -O ihex $< $@

# Signed one-based source indices corresponding to the Fix42 release manifest.
$(BUILD)/transmitter-fix42-normal.elf: AXES := -DPUCK_AXIS_X=2 -DPUCK_AXIS_Y=-3 -DPUCK_AXIS_Z=-1
$(BUILD)/transmitter-fix42-inverted.elf: AXES := -DPUCK_AXIS_X=-2 -DPUCK_AXIS_Y=-3 -DPUCK_AXIS_Z=1
$(BUILD)/transmitter-fix42-handheld.elf: AXES := -DPUCK_AXIS_X=2 -DPUCK_AXIS_Y=1 -DPUCK_AXIS_Z=-3

$(BUILD)/transmitter-fix42-%.elf: transmitter/fix42.c transmitter/patch.ld $(wildcard common/*.h) $(BUILD)/radio-settings | $(BUILD)
	$(CROSS)gcc $(FLAGS) $(BSP) $(APP_WARN) $(AXES) -ffreestanding -nostdlib \
	    -Wl,--gc-sections -T transmitter/patch.ld $< -o $@

$(BUILD)/transmitter-fix42-%.uf2: $(DEPS_DIR)/fix42/fix42-ds4-%.uf2 $(BUILD)/transmitter-fix42-%.elf $(BUILD)/transmitter-fix42-%.bin tools/patch_fix42.py tools/uf2.py
	$(PYTHON) tools/patch_fix42.py --nm $(CROSS)nm $* $(word 1,$^) $(word 2,$^) $(word 3,$^) $@

$(BUILD)/test-ds4: tests/test_ds4.c receiver/ds4.c receiver/ds4.h common/protocol.h | $(BUILD)
	$(HOST_CC) -std=c11 $(APP_WARN) -Icommon -Ireceiver tests/test_ds4.c receiver/ds4.c -o $@

check: all $(BUILD)/test-ds4
	$(BUILD)/test-ds4
	$(PYTHON) tests/test_images.py "$(BUILD)" "$(DEPS_DIR)" "$(CROSS)nm"

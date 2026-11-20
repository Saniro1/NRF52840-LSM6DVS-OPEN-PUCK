#include "tusb.h"
#include "nrf.h"
#include "ds4.h"

/* USB DS4 identity matches the wired Fix42 parser path in JoyShockMapper. */
static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x054c,
    .idProduct = 0x05c4,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

static const uint8_t report_descriptor[] = {
    0x05, 0x01,       /* Generic Desktop */
    0x09, 0x05,       /* Game Pad */
    0xa1, 0x01,       /* Application collection */
    0x85, DS4_REPORT_ID,
    0x06, 0x00, 0xff, /* Vendor data with DS4 report layout. */
    0x09, 0x01,
    0x15, 0x00,
    0x26, 0xff, 0x00,
    0x75, 0x08,
    0x95, DS4_REPORT_SIZE - 1,
    0x81, 0x02,
    0x85, 0x05,       /* Accept DS4 USB output writes. */
    0x09, 0x02,
    0x95, 31,
    0x91, 0x02,
    0x85, DS4_FEATURE_CALIBRATION,
    0x09, 0x03,
    0x95, DS4_CALIBRATION_PAYLOAD,
    0xb1, 0x02,
    0x85, DS4_FEATURE_PAIRING_INFO,
    0x09, 0x04,
    0x95, DS4_PAIRING_PAYLOAD,
    0xb1, 0x02,
    0x85, DS4_FEATURE_FIRMWARE_INFO,
    0x09, 0x05,
    0x95, DS4_FIRMWARE_PAYLOAD,
    0xb1, 0x02,
    0xc0,
};

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(report_descriptor),
                       0x81, DS4_REPORT_SIZE, 1),
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return (const uint8_t *)&device_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    return index == 0 ? configuration_descriptor : NULL;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return instance == 0 ? report_descriptor : NULL;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    static uint16_t utf16[32];
    static const char *const strings[] = {"", "Open Puck", "Open Puck Wireless Gyro"};
    static const char hex[] = "0123456789ABCDEF";
    unsigned count = 0;

    if (index == 0) {
        utf16[1] = 0x0409;
        count = 1;
    } else if (index == 3) {
        for (unsigned word = 0; word < 2; ++word) {
            uint32_t id = NRF_FICR->DEVICEID[word];
            for (unsigned digit = 0; digit < 8; ++digit) {
                utf16[++count] = (uint16_t)hex[(id >> (28u - 4u * digit)) & 15u];
            }
        }
    } else if (index < sizeof(strings) / sizeof(strings[0])) {
        const char *str = strings[index];
        while (*str != '\0') {
            utf16[++count] = (uint8_t)*str++;
        }
    } else {
        return NULL;
    }
    utf16[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2u * (count + 1u)));
    return utf16;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t type, uint8_t *buffer, uint16_t size)
{
    (void)instance;
    if (type != HID_REPORT_TYPE_FEATURE) {
        return 0;
    }
    return ds4_build_feature(report_id, NRF_FICR->DEVICEID[0], NRF_FICR->DEVICEID[1],
                             buffer, size);
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t type,
                           uint8_t const *buffer, uint16_t size)
{
    (void)instance;
    (void)report_id;
    (void)type;
    (void)buffer;
    (void)size;
}

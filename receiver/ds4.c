#include <string.h>
#include "ds4.h"

static void write_le16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

bool puck_receive(puck_link_t *link, const puck_packet_t *packet, uint32_t now_ms)
{
    if (packet->magic != PUCK_PACKET_MAGIC) {
        return false;
    }
    link->latest = *packet;
    link->last_rx_ms = now_ms;
    link->active = true;
    return true;
}

void ds4_build_input(puck_link_t *link, uint32_t now_ms, uint8_t sequence,
                     uint8_t report[DS4_REPORT_SIZE])
{
    if (link->active && now_ms - link->last_rx_ms >= PUCK_LINK_TIMEOUT_MS) {
        link->active = false;
    }
    memset(report, 0, DS4_REPORT_SIZE);
    report[0] = DS4_REPORT_ID;
    report[1] = report[2] = report[3] = report[4] = 128;
    report[5] = 8; /* Neutral hat, all buttons released. */
    report[7] = (uint8_t)(sequence << 2);
    write_le16(&report[10], (uint16_t)(now_ms * 375u / 2u)); /* 16/3 us per tick. */
    for (unsigned axis = 0; axis < 3; ++axis) {
        write_le16(&report[13 + 2 * axis], link->active ? (uint16_t)link->latest.gyro[axis] : 0);
        write_le16(&report[19 + 2 * axis], (uint16_t)link->latest.accel[axis]);
    }
    report[30] = 0x1b; /* USB power and full battery. */
    report[35] = report[39] = 0x80; /* Both touch contacts inactive. */
}

uint16_t ds4_build_feature(uint8_t id, uint32_t device_low, uint32_t device_high,
                          uint8_t *payload, uint16_t capacity)
{
    uint16_t length;
    switch (id) {
    case DS4_FEATURE_CALIBRATION:
        length = DS4_CALIBRATION_PAYLOAD;
        break;
    case DS4_FEATURE_PAIRING_INFO:
        length = DS4_PAIRING_PAYLOAD;
        break;
    case DS4_FEATURE_FIRMWARE_INFO:
        length = DS4_FIRMWARE_PAYLOAD;
        break;
    default:
        return 0;
    }
    uint8_t feature[DS4_FIRMWARE_PAYLOAD] = {0};
    if (id == DS4_FEATURE_CALIBRATION) {
        /* The sender already calibrates motion. Describe its nominal scales
         * to host drivers that apply feature-report calibration.
         */
        for (unsigned axis = 0; axis < 3; ++axis) {
            write_le16(&feature[6 + 4 * axis], 16000);
            write_le16(&feature[8 + 4 * axis], (uint16_t)-16000);
            write_le16(&feature[22 + 4 * axis], 8192);
            write_le16(&feature[24 + 4 * axis], (uint16_t)-8192);
        }
        write_le16(&feature[18], 1000);
        write_le16(&feature[20], 1000);
    } else if (id == DS4_FEATURE_PAIRING_INFO) {
        for (unsigned byte = 0; byte < 4; ++byte) {
            feature[byte] = (uint8_t)(device_low >> (8 * byte));
        }
        feature[4] = (uint8_t)device_high;
        feature[5] = ((uint8_t)(device_high >> 8) & 0xfcu) | 2u;
    } else {
        write_le16(&feature[34], 1); /* Puck hardware protocol generation. */
        write_le16(&feature[40], 0x0100); /* Puck firmware 0.1.0. */
    }
    length = capacity < length ? capacity : length;
    memcpy(payload, feature, length);
    return length;
}

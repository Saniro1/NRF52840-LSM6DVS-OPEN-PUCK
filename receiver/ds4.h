#pragma once

#include <stdbool.h>
#include "protocol.h"

#define DS4_REPORT_SIZE 64
#define DS4_REPORT_ID 1
#define DS4_FEATURE_CALIBRATION 0x02
#define DS4_FEATURE_PAIRING_INFO 0x12
#define DS4_FEATURE_FIRMWARE_INFO 0xa3
#define DS4_CALIBRATION_PAYLOAD 36
#define DS4_PAIRING_PAYLOAD 15
#define DS4_FIRMWARE_PAYLOAD 48

typedef struct {
    puck_packet_t latest;
    bool active;
    uint32_t last_rx_ms;
} puck_link_t;

bool puck_receive(puck_link_t *link, const puck_packet_t *packet, uint32_t now_ms);
void ds4_build_input(puck_link_t *link, uint32_t now_ms, uint8_t sequence,
                     uint8_t report[DS4_REPORT_SIZE]);
uint16_t ds4_build_feature(uint8_t id, uint32_t device_low, uint32_t device_high,
                          uint8_t *payload, uint16_t capacity);

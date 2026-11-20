#pragma once

#include <stddef.h>
#include <stdint.h>

#define PUCK_PACKET_MAGIC 0x5041u
#define PUCK_LINK_TIMEOUT_MS 30u

/* Native little-endian Cortex-M layout. Both RADIO DMA buffers are word-aligned. */
typedef struct {
    uint16_t magic;
    uint16_t sequence;
    int16_t gyro[3];
    int16_t accel[3];
} puck_packet_t;

_Static_assert(sizeof(puck_packet_t) == 16, "Unexpected radio packet size");
_Static_assert(offsetof(puck_packet_t, gyro) == 4, "Unexpected gyro offset");
_Static_assert(offsetof(puck_packet_t, accel) == 10, "Unexpected accel offset");

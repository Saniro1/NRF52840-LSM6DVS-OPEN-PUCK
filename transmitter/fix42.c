#include <limits.h>
#include <stdbool.h>
#include "radio.h"

/* Verified addresses in the three SHA-256-pinned Fix42 images.
 * The ESB thread is replaced and its tx_errors word serves as a publication counter.
 */
#define MOTION_ACCEL ((volatile const int16_t *)0x20005106u)
#define MOTION_GYRO ((volatile const int16_t *)0x2000510cu)
#define MOTION_GENERATION (*(volatile uint32_t *)0x20003274u)
#define ORIGINAL_WRITER ((void (*)(int, const void *, int, const void *))0xacc01u)
#define CLOCK_START ((int (*)(void))0x8b9du)
#define UPTIME_TICKS ((uint64_t (*)(void))0x2faf9u)
#define SLEEP_TICKS ((void (*)(int64_t))0x2a20du)
#define SENSOR_TIMEOUT_TICKS 1638u /* 50 ms at 32,768 ticks/s. */

#ifndef PUCK_AXIS_X
#error "Select a Fix42 mounting profile"
#endif

/* Call exactly the existing Fix42 writer, then record a valid gyro publication. */
__attribute__((section(".text.puck_publish")))
void puck_publish(int gyro_valid, const void *gyro, int accel_valid, const void *accel)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ORIGINAL_WRITER(gyro_valid, gyro, accel_valid, accel);
    if (gyro_valid) {
        ++MOTION_GENERATION;
    }
    __set_PRIMASK(primask);
}

static int16_t mapped_axis(const int16_t axes[3], int axis)
{
    int32_t value = axis > 0 ? axes[axis - 1] : -(int32_t)axes[-axis - 1];
    return value > INT16_MAX ? INT16_MAX : (int16_t)value;
}

static uint32_t snapshot(puck_packet_t *packet)
{
    int16_t gyro[3], accel[3];
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t generation = MOTION_GENERATION;
    for (unsigned axis = 0; axis < 3; ++axis) {
        gyro[axis] = MOTION_GYRO[axis];
        accel[axis] = MOTION_ACCEL[axis];
    }
    __set_PRIMASK(primask);

    packet->gyro[0] = mapped_axis(gyro, PUCK_AXIS_X);
    packet->gyro[1] = mapped_axis(gyro, PUCK_AXIS_Y);
    packet->gyro[2] = mapped_axis(gyro, PUCK_AXIS_Z);
    packet->accel[0] = mapped_axis(accel, PUCK_AXIS_X);
    packet->accel[1] = mapped_axis(accel, PUCK_AXIS_Y);
    packet->accel[2] = mapped_axis(accel, PUCK_AXIS_Z);
    return generation;
}

/* Zephyr static ESB thread entry. The original native ESB stack never starts. */
__attribute__((section(".text.puck_transmit"), noreturn))
void puck_transmit(void)
{
    while (CLOCK_START() != 0) {
        SLEEP_TICKS(3277); /* Allow the clock manager to settle before retrying. */
    }

    puck_radio_init();
    _Alignas(4) puck_packet_t packet;
    packet.magic = PUCK_PACKET_MAGIC;
    packet.sequence = 0;
    NRF_RADIO->PACKETPTR = (uintptr_t)&packet;
    uint32_t deadline = (uint32_t)UPTIME_TICKS();
    uint32_t published_at = deadline;
    uint32_t last_generation = 0;
    bool sensor_active = false;
    unsigned fraction = 0;

    for (;;) {
        uint32_t now = (uint32_t)UPTIME_TICKS();
        int32_t wait = (int32_t)(deadline - now);
        if (wait > 0) {
            /* Zephyr's relative timeout adds a tick on this image. */
            SLEEP_TICKS(wait > 1 ? wait - 1 : 1);
            continue;
        }
        if ((uint32_t)(now - deadline) >= 32u) {
            deadline = now;
        }
        /* Alternate 32 and 33 ticks for an average of 32.768 ticks/packet. */
        fraction += 768u;
        deadline += 32u;
        if (fraction >= 1000u) {
            fraction -= 1000u;
            ++deadline;
        }

        if (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {
            /* A missed hardware end can never cause us to overwrite live DMA. */
            NRF_RADIO->TASKS_DISABLE = 1;
            continue;
        }

        uint32_t generation = snapshot(&packet);
        if (generation != last_generation) {
            last_generation = generation;
            published_at = now;
            sensor_active = true;
        }
        if (sensor_active && (uint32_t)(now - published_at) < SENSOR_TIMEOUT_TICKS) {
            ++packet.sequence;
            __DMB();
            NRF_RADIO->TASKS_TXEN = 1;
        } else {
            sensor_active = false;
        }
    }
}

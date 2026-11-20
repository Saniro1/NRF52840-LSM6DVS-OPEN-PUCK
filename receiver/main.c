#include "ds4.h"
#include "radio.h"
#include "tusb.h"

enum usb_power_event {
    USB_POWER_DETECTED,
    USB_POWER_REMOVED,
    USB_POWER_READY,
};

extern void tusb_hal_nrf_power_event(uint32_t event);
extern uint32_t __isr_vector;
static volatile uint32_t uptime_ms;
static _Alignas(4) puck_packet_t radio_packet;
static puck_link_t link;

void SysTick_Handler(void)
{
    ++uptime_ms;
}

uint32_t tusb_time_millis_api(void)
{
    return uptime_ms;
}

void USBD_IRQHandler(void)
{
    tud_int_handler(0);
}

static void radio_listen(void)
{
    NRF_RADIO->EVENTS_DISABLED = 0;
    (void)NRF_RADIO->EVENTS_DISABLED;
    NRF_RADIO->TASKS_RXEN = 1;
}

static void poll_radio(void)
{
    if (NRF_RADIO->EVENTS_DISABLED == 0) {
        return;
    }
    __DMB();
    if (NRF_RADIO->CRCSTATUS == RADIO_CRCSTATUS_CRCSTATUS_CRCOk) {
        puck_receive(&link, &radio_packet, uptime_ms);
    }
    radio_listen();
}

int main(void)
{
    /* The Nordic startup does not relocate VTOR when entered via a bootloader. */
    SCB->VTOR = (uintptr_t)&__isr_vector;
    __DSB();
    __ISB();
    SysTick_Config(SystemCoreClock / 1000u);
    NVIC_SetPriority(USBD_IRQn, 2);
    const tusb_rhport_init_t usb = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };
    tusb_init(0, &usb);

    puck_radio_init();
    NRF_RADIO->PACKETPTR = (uintptr_t)&radio_packet;

    bool vbus = false;
    bool usb_power_ready = false;
    uint8_t sequence = 0;
    for (;;) {
        uint32_t power = NRF_POWER->USBREGSTATUS;
        bool present = (power & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
        if (present && !vbus) {
            tusb_hal_nrf_power_event(USB_POWER_DETECTED);
        } else if (!present && vbus) {
            NRF_RADIO->TASKS_DISABLE = 1;
            while (NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled) {}
            link = (puck_link_t){0};
            tusb_hal_nrf_power_event(USB_POWER_REMOVED);
            usb_power_ready = false;
        }
        vbus = present;

        if (vbus && !usb_power_ready && (power & POWER_USBREGSTATUS_OUTPUTRDY_Msk)) {
            tusb_hal_nrf_power_event(USB_POWER_READY); /* Also waits for the crystal. */
            usb_power_ready = true;
            radio_listen();
        }
        if (usb_power_ready) {
            poll_radio();
        }
        tud_task();
        if (usb_power_ready && tud_hid_ready()) {
            uint8_t report[DS4_REPORT_SIZE];
            ds4_build_input(&link, uptime_ms, sequence, report);
            if (tud_hid_report(DS4_REPORT_ID, &report[1], sizeof(report) - 1u)) {
                ++sequence;
            }
        }
    }
}

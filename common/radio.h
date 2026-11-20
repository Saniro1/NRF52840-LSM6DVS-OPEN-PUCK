#pragma once

#include "nrf.h"
#include "protocol.h"

#ifndef PUCK_CHANNEL
#define PUCK_CHANNEL 76
#endif
#ifndef PUCK_ADDRESS_BASE
#define PUCK_ADDRESS_BASE 0xb7519d00u
#endif

/* Keep Nordic's channel number in the 2.400 GHz band with modulation headroom. */
_Static_assert(PUCK_CHANNEL >= 2 && PUCK_CHANNEL <= 80, "Invalid radio channel");
_Static_assert((PUCK_ADDRESS_BASE & 0xffu) == 0, "Base address uses upper three bytes");

static inline void puck_radio_init(void)
{
    NRF_RADIO->INTENCLR = UINT32_MAX;
    NRF_RADIO->MODE = RADIO_MODE_MODE_Nrf_2Mbit;
    NRF_RADIO->MODECNF0 = RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos;
    NRF_RADIO->FREQUENCY = PUCK_CHANNEL;
    NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_0dBm;
    NRF_RADIO->BASE0 = PUCK_ADDRESS_BASE;
    NRF_RADIO->PREFIX0 = 0xe3u;
    NRF_RADIO->TXADDRESS = 0;
    NRF_RADIO->RXADDRESSES = 1;
    NRF_RADIO->PCNF0 = RADIO_PCNF0_PLEN_8bit << RADIO_PCNF0_PLEN_Pos;
    NRF_RADIO->PCNF1 = (sizeof(puck_packet_t) << RADIO_PCNF1_MAXLEN_Pos) |
                       (sizeof(puck_packet_t) << RADIO_PCNF1_STATLEN_Pos) |
                       (3u << RADIO_PCNF1_BALEN_Pos) |
                       (RADIO_PCNF1_WHITEEN_Enabled << RADIO_PCNF1_WHITEEN_Pos);
    NRF_RADIO->DATAWHITEIV = (0x40u | PUCK_CHANNEL) & 0x7fu;
    NRF_RADIO->CRCCNF = RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos;
    NRF_RADIO->CRCPOLY = 0x11021u;
    NRF_RADIO->CRCINIT = 0xffffu;
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
}

#include <assert.h>
#include "ds4.h"

static uint16_t le16(const uint8_t *bytes)
{
    return (uint16_t)(bytes[0] | (bytes[1] << 8));
}

int main(void)
{
    puck_link_t link = {0};
    uint8_t report[DS4_REPORT_SIZE];

    ds4_build_input(&link, 0, 0, report);
    assert(report[0] == DS4_REPORT_ID);
    assert(report[1] == 128 && report[4] == 128 && report[5] == 8);
    assert(le16(report + 13) == 0 && le16(report + 19) == 0);

    puck_packet_t packet = {
        .magic = PUCK_PACKET_MAGIC,
        .sequence = UINT16_MAX,
        .gyro = {16, -32, INT16_MIN},
        .accel = {-5, 8192, INT16_MAX},
    };
    assert(puck_receive(&link, &packet, UINT32_MAX - 10));
    ds4_build_input(&link, 18, 63, report);
    assert(report[7] == 252 && le16(&report[10]) == 3375);
    assert(le16(&report[13]) == 16 && le16(&report[15]) == (uint16_t)-32);
    assert(le16(&report[17]) == (uint16_t)INT16_MIN);
    assert(le16(&report[19]) == (uint16_t)-5);
    assert(le16(&report[21]) == 8192 && le16(&report[23]) == INT16_MAX);
    assert(report[30] == 0x1b && report[35] == 0x80 && report[39] == 0x80);

    packet.magic = 0;
    packet.gyro[0] = 100;
    assert(!puck_receive(&link, &packet, 19));
    ds4_build_input(&link, 19, 64, report);
    assert(!link.active && report[7] == 0);
    assert(le16(&report[13]) == 0 && le16(&report[15]) == 0 && le16(&report[17]) == 0);
    assert(le16(&report[21]) == 8192);
    ds4_build_input(&link, UINT32_MAX - 5, 0, report);
    assert(le16(&report[13]) == 0);

    packet.magic = PUCK_PACKET_MAGIC;
    packet.sequence = 0;
    assert(puck_receive(&link, &packet, 20));
    ds4_build_input(&link, 20, 1, report);
    assert(link.active && le16(&report[13]) == 100);

    uint8_t feature[DS4_FIRMWARE_PAYLOAD];
    assert(ds4_build_feature(DS4_FEATURE_CALIBRATION, 0, 0, feature, sizeof(feature)) == 36);
    assert(le16(feature) == 0 && le16(feature + 6) == 16000);
    assert(le16(feature + 8) == (uint16_t)-16000);
    assert(le16(feature + 18) == 1000 && le16(feature + 20) == 1000);
    assert(le16(feature + 22) == 8192 && le16(feature + 32) == (uint16_t)-8192);
    assert(ds4_build_feature(DS4_FEATURE_PAIRING_INFO, 0x04030201, 0x0605,
                             feature, sizeof(feature)) == 15);
    assert(feature[0] == 1 && feature[3] == 4 && feature[4] == 5 && feature[5] == 6);
    assert(ds4_build_feature(DS4_FEATURE_FIRMWARE_INFO, 0, 0, feature, sizeof(feature)) == 48);
    assert(le16(feature + 34) == 1 && le16(feature + 40) == 0x0100);
    assert(ds4_build_feature(DS4_FEATURE_CALIBRATION, 0, 0, feature, 4) == 4);
    assert(ds4_build_feature(0xff, 0, 0, feature, sizeof(feature)) == 0);
}

#include "packets.h"

const std::string &button_name(BUTTON button) {
    switch (button) {
        case BUTTON::NONE:
            return STRBUTTON_NONE;
        case BUTTON::RIGHT:
            return STRBUTTON_RIGHT;
        case BUTTON::DOWN:
            return STRBUTTON_DOWN;
        case BUTTON::LEFT:
            return STRBUTTON_LEFT;
        case BUTTON::UP:
            return STRBUTTON_UP;
        case BUTTON::SELECT:
            return STRBUTTON_SELECT;
        case BUTTON::START:
            return STRBUTTON_START;
        case BUTTON::B:
            return STRBUTTON_B;
        case BUTTON::A:
            return STRBUTTON_A;
    }
}

const Status Status::decode_from_packet(const StatusPacket &packet) {
    assert (packet.base.type == (uint8_t)PACKET_TYPE::STATUS);

    return Status(
            MacAddress(packet.base.mac.mac),
            packet.version,
            (int8_t)packet.rssi - 128,
            MacAddress(packet.bssid.mac),
            packet.gpio_state,
            packet.last_button,
            packet.button_down,
            ntohs(packet.system_voltage),
            ntohs(packet.update_count),
            ntohs(packet.heap_free),
            packet.sleep_perf,
            ntohl(packet.time)
    );
}


const Scan Scan::decode_from_packet(const ScanPacket &packet) {
    assert (packet.base.type == (uint8_t)PACKET_TYPE::SCAN);

    std::vector<ScanStation> stations;
    const ScanData *scans = (ScanData*)(&packet + sizeof(ScanPacket));
    for (; scans < scans + packet.station_count; scans++) {
        stations.emplace_back(MacAddress(scans->bssid.mac),
                              (int8_t)scans->rssi - 128,
                              scans->channel);
    }

    return Scan(MacAddress(packet.base.mac.mac),
                packet.timestamp,
                std::move(stations));
}

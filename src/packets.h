#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <stdbool.h>
#include <cstdio>
#include <string>
#include <assert.h>
#include <memory.h>

#include <arpa/inet.h>
#include <vector>
#include <sstream>

#define PACKED __attribute__ ((packed))

enum BUTTON_BIT {
    RIGHT = 0x01,
    DOWN = 0x02,
    LEFT = 0x04,
    UP = 0x08,
    SELECT = 0x10,
    START = 0x20,
    B = 0x30,
    A = 0x40,
};

enum class BUTTON {
    NONE = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3,
    UP = 4,
    SELECT = 5,
    START = 6,
    B = 7,
    A = 8,
};

enum PACKET_TYPE {
    STATUS = 0x01,
    LIGHTS = 0x02,
    LIGHTS_RSSI = 0x03,
    SCAN_REQUEST = 0x04,
    SCAN = 0x05,
      
    LIGHTS_RAINBOW = 0x07,
    CONFIG = 0x08,
    DEEP_SLEEP = 0x09,
      
    STATUS_REQUEST = 0x11,
};

const char button_char(BUTTON b);

static const std::string &STRBUTTON_NONE("none");
static const std::string STRBUTTON_RIGHT("right");
static const std::string STRBUTTON_DOWN("down");
static const std::string STRBUTTON_LEFT("left");
static const std::string STRBUTTON_UP("up");
static const std::string STRBUTTON_SELECT("select");
static const std::string STRBUTTON_START("start");
static const std::string STRBUTTON_B("b");
static const std::string STRBUTTON_A("a");

const std::string &button_name(BUTTON button);

struct PACKED MacAddressData {
    uint8_t mac[6];
};

struct PACKED BasePacket {
    MacAddressData mac;
    uint8_t type;
};


struct PACKED StatusPacket {
    BasePacket base;
  
    uint8_t version;
    uint8_t rssi;
    MacAddressData bssid;
    uint8_t gpio_state;
    uint8_t last_button;
    uint8_t button_down;
    uint8_t average_led_pwr;
    uint16_t system_voltage;
    uint16_t update_count;
    uint16_t heap_free;
    uint8_t sleep_perf;
    uint8_t __reserved;
    uint32_t time;
};

struct PACKED LightData {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
};

struct PACKED LightsPacket {
    BasePacket base;

    uint8_t match;
    uint8_t mask;
    uint8_t __reserved;
    LightData lights[4];
};

struct PACKED LightsRssiPacket {
    BasePacket base;

    uint8_t min_rssi;
    uint8_t max_rssi;
    uint8_t led_intensity;
};

struct PACKED ScanRequestPacket {
    BasePacket base;
};

struct PACKED ScanData {
    MacAddressData bssid;
    uint8_t rssi;
    uint8_t channel;
};

struct PACKED ScanPacket {
    BasePacket base;

    uint32_t timestamp;
    uint8_t station_count;
    // <station_count> ScanData are here
};

struct PACKED LightsRainbowPacket {
    BasePacket base;
  
    uint8_t __reserved1;
    uint8_t __reserved2;
    uint8_t __reserved3;
    uint16_t runtime;
    uint8_t speed;
    uint8_t intensity;
    uint8_t offset;
};

struct PACKED ConfigurePacket {
    BasePacket base;

    uint8_t requested_update;
    uint8_t disable_push;
    uint8_t disable_raw;
    uint8_t raw_rate; // 0x1a is recommended
};

#undef PACKED

class MacAddress {
    uint8_t _data[6];

public:
    MacAddress(const uint8_t *data) {
        memcpy(_data, data, 6);
    }

    operator uint64_t() const {
        return ((uint64_t)_data[0] << 40) | ((uint64_t)_data[1] << 32) | ((uint64_t)_data[2] << 24) | ((uint64_t)_data[3] << 16) | ((uint64_t)_data[4] << 8) | _data[5];
    }

    operator std::string() const {
        char buf[18];
        sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
                _data[0], _data[1], _data[2], _data[3], _data[4], _data[5]);
        buf[17] = '\0';
        return std::string(buf);
    }

    static MacAddress null_mac() {
        static uint8_t null_mac_data[6] {0, 0, 0, 0, 0, 0};
        static MacAddress null_mac(null_mac_data);

        return null_mac;
    }
};

class Status {
    int _version;
    MacAddress _mac;
    int8_t _rssi;
    MacAddress _bssid;
  
    uint8_t _gpio_state;
    BUTTON _last_button;
    bool _button_down;

    uint16_t _system_voltage;
    uint16_t _update_count;
    uint16_t _heap_free;
    uint8_t _sleep_performance;
    uint32_t _time;

    Status(MacAddress mac,
           int version,
           int8_t rssi,
           MacAddress bssid,
           uint8_t gpio_state,
           uint8_t last_button,
           bool button_down,
           uint16_t system_voltage,
           uint16_t update_count,
           uint16_t heap_free,
           uint8_t sleep_performance,
           uint32_t time)
            : _mac(mac),
              _version(version),
              _rssi(rssi),
              _bssid(bssid),
              _gpio_state(gpio_state),
              _last_button((BUTTON)last_button),
              _button_down(button_down),
              _system_voltage(system_voltage),
              _update_count(update_count),
              _heap_free(heap_free),
              _sleep_performance(sleep_performance),
              _time(time) {}

public:
    static const Status decode_from_packet(const StatusPacket *packet);

    const MacAddress &mac_address() const { return _mac; }
    int               version()     const { return _version; }
    int8_t            rssi()        const { return _rssi; }
    const MacAddress &bssid()       const { return _bssid; }
    uint8_t           gpio_state()  const { return _gpio_state; }

    bool              held_right()  const { return (_gpio_state & BUTTON_BIT::RIGHT) != 0; }
    bool              held_down()   const { return (_gpio_state & BUTTON_BIT::DOWN) != 0; }
    bool              held_left()   const { return (_gpio_state & BUTTON_BIT::LEFT) != 0; }
    bool              held_up()     const { return (_gpio_state & BUTTON_BIT::UP) != 0; }
    bool              held_select() const { return (_gpio_state & BUTTON_BIT::SELECT) != 0; }
    bool              held_start()  const { return (_gpio_state & BUTTON_BIT::START) != 0; }
    bool              held_b()      const { return (_gpio_state & BUTTON_BIT::B) != 0; }
    bool              held_a()      const { return (_gpio_state & BUTTON_BIT::A) != 0; }

    bool              button_down() const { return _button_down; }
    BUTTON            last_button() const { return _last_button; }
    const std::string &last_button_name() const { return button_name(_last_button); }

    uint16_t          system_voltage() const { return _system_voltage; }
    uint16_t          update_count() const { return _update_count; }
    uint16_t          heap_free()   const { return _heap_free; }
    uint8_t           sleep_performance() const { return _sleep_performance; }
    uint32_t          time()        const { return _time; }

    friend std::ostream& operator<< (std::ostream &stream, const Status &status) {
        std::ostringstream str;
        str << "< Status: "
            << "[" << std::string(status.mac_address()) << "] "
            << "#" << status.update_count();

        if (status.last_button() != BUTTON::NONE || status.gpio_state() != 0) {
            str << " " << status.last_button_name();
            if (status.button_down()) {
                str << " PRESS";
            } else {
                str << " RELEASE";
            }
        }

        str << " >";
        stream << str.str();
        return stream;
    }
};

class ScanStation {
    MacAddress _mac;
    int8_t _rssi;
    uint8_t _channel;

public:
    ScanStation(MacAddress mac,
                int8_t rssi,
                uint8_t channel) :
            _mac(mac),
            _rssi(rssi),
            _channel(channel) {}

    const MacAddress &mac()     const { return _mac; }
    int8_t            rssi()    const { return _rssi;}
    uint8_t           channel() const { return _channel;}
};

class Scan {
    MacAddress _mac;
    uint32_t _timestamp;
    std::vector<ScanStation> _stations;

    Scan(MacAddress mac,
         uint32_t timestamp,
         const std::vector<ScanStation> &&stations)
            : _mac(mac),
              _timestamp(timestamp),
              _stations(std::move(stations)) {}

public:
    static const Scan decode_from_packet(const ScanPacket *packet);

    Scan()
    : _mac(MacAddress::null_mac()),
      _timestamp(0),
      _stations() {}

    const MacAddress &mac_address() const { return _mac; }
    const uint32_t    timestamp()   const { return _timestamp; }
    const std::vector<ScanStation> &stations() const { return _stations; }

    bool update(const Scan &other) {
        if (other._timestamp == _timestamp) {
            _stations.insert(_stations.end(), other._stations.begin(), other._stations.end());
            return true;
        }

        return false;
    }

    friend std::ostream& operator<< (std::ostream &stream, const Scan &scan) {
        std::ostringstream str;
        str << "< Scan: "
            << "[" << std::string(scan.mac_address()) << "] "
            << scan._stations.size() << " stations" << std::endl;

        for (auto &station : scan.stations()) {
            str << "    " << std::string(station.mac())
                << " " << std::to_string(station.rssi())
                << " (Channel " << std::to_string(station.channel()) << ")" << std::endl;
        }

        str << " >";
        stream << str.str();
        return stream;
    }
};

#endif

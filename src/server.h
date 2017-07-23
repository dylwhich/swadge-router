#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <functional>
#include <map>

#include "packets.h"

class Server;

class BadgeInfo {
    Server *_server;

    struct sockaddr_in _sockaddr;
    socklen_t _sockaddr_len;

    std::string _host;

    Status _last_status;
    uint64_t _route;

    Scan _last_scan;

    void *_app_data;

public:
    BadgeInfo(Server *server,
              struct sockaddr_in &sockaddr,
              socklen_t sockaddr_len,
              const std::string &host,
              const Status &&status,
              uint64_t route = 0)
            : _server(server),
              _sockaddr(sockaddr),
              _sockaddr_len(sockaddr_len),
              _host(host),
              _last_status(status),
              _route(route),
              _last_scan(),
              _app_data(nullptr) {}

    struct sockaddr_in &sock_address() { return _sockaddr; }
    socklen_t sock_address_len() { return _sockaddr_len; }

    void set_address(struct sockaddr_in address, socklen_t len) {
        _sockaddr = address;
        _sockaddr_len = len;
    }

    const std::string &host() { return _host; }

    Status &last_status() { return _last_status; }

    void set_last_status(const Status &&status) {
        _last_status = status;
    }

    uint64_t route() { return _route; }

    void set_route(uint64_t route) {
        _route = route;
    }

    void *app_data() {
        return _app_data;
    }

    void on_scan(const Scan &scan) {
        if (!_last_scan.update(scan)) {
            _last_scan = scan;
        }
    }

    const Scan &last_scan() {
        return _last_scan;
    }

    void scan();
    void set_lights(uint8_t r1, uint8_t g1, uint8_t b1,
                    uint8_t r2, uint8_t g2, uint8_t b2,
                    uint8_t r3, uint8_t g3, uint8_t b3,
                    uint8_t r4, uint8_t g4, uint8_t b4,
                    uint8_t mask = 0, uint8_t match = 0);
};

using ScanCallback = std::function<void(const Scan&)>;
using StatusCallback = std::function<void(const Status&)>;


class Server {
    int _sockfd;
    bool _running;

    ScanCallback _scan_callback;
    StatusCallback _status_callback;


    std::map <uint64_t, BadgeInfo> _badge_ips;

public:
    Server()
            : _sockfd(-1),
              _running(false) {}

    void set_on_scan(ScanCallback cb) {
        _scan_callback = cb;
    }

    void set_on_status(StatusCallback cb) {
        _status_callback = cb;
    }

    void handle_data(struct sockaddr_in &address, const char *data, ssize_t len);

    void send_packet(BadgeInfo *badge, const char *packet, size_t packet_len);
    void send_packet(BadgeInfo &badge, const char *packet, size_t packet_len);
    void send_packet(MacAddress &mac, const char *packet, size_t packet_len);
    void send_packet(uint64_t mac, const char *packet, size_t packet_len);

    void run();
};

#endif


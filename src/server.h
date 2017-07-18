#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <map>

#include "packets.h"

class BadgeInfo {
    struct sockaddr_in _sockaddr;
    socklen_t _sockaddr_len;

    std::string _host;

    Status _last_status;
    uint64_t _route;

    void *_app_data;

public:
    BadgeInfo(struct sockaddr_in &sockaddr,
              socklen_t sockaddr_len,
              const std::string &host,
              const Status &&status,
              uint64_t route = 0)
            : _sockaddr(sockaddr),
              _sockaddr_len(sockaddr_len),
              _host(host),
              _last_status(status),
              _route(route) {}

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
};

class Server {
    int _sockfd;
    bool _running;

    std::map <uint64_t, BadgeInfo> _badge_ips;

public:
    Server()
            : _sockfd(-1),
              _running(false) {}

    void handle_data(struct sockaddr_in &address, const char *data, ssize_t len);

    void send_packet(BadgeInfo &badge, const char *packet, size_t packet_len);
    void send_packet(MacAddress &mac, const char *packet, size_t packet_len);
    void send_packet(uint64_t mac, const char *packet, size_t packet_len);

    void run();
};

#endif


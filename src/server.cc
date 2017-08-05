#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

#include "packets.h"
#include "server.h"

#define BUFSIZE 1024
#define PORT 8000

void set_mac_address(uint8_t *data, uint64_t mac) {
    data[0] = (uint8_t)((mac >> 40) & 0xff);
    data[1] = (uint8_t)((mac >> 32) & 0xff);
    data[2] = (uint8_t)((mac >> 24) & 0xff);
    data[3] = (uint8_t)((mac >> 16) & 0xff);
    data[4] = (uint8_t)((mac >> 8) & 0xff);
    data[5] = (uint8_t)(mac & 0xff);
}

void BadgeInfo::scan() {
    ScanRequestPacket packet{};
    packet.base.type = SCAN_REQUEST;
    set_mac_address(packet.base.mac.mac, _mac);

    _server->send_packet(this, (char*)&packet, sizeof(ScanPacket));
}

void BadgeInfo::set_lights(uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2,
                           uint8_t r3, uint8_t g3, uint8_t b3,
                           uint8_t r4, uint8_t g4, uint8_t b4,
                           uint8_t mask, uint8_t match) {
    LightsPacket packet{};
    packet.base.type = LIGHTS;
    set_mac_address(packet.base.mac.mac, _mac);

    packet.mask = mask;
    packet.match = match;

    packet.lights[0].red   = r1;
    packet.lights[0].green = g1;
    packet.lights[0].blue  = b1;

    packet.lights[1].red   = r2;
    packet.lights[1].green = g2;
    packet.lights[1].blue  = b2;

    packet.lights[2].red   = r3;
    packet.lights[2].green = g3;
    packet.lights[2].blue  = b3;

    packet.lights[3].red   = r4;
    packet.lights[3].green = g4;
    packet.lights[3].blue  = b4;

    _server->send_packet(this, (char*)&packet, sizeof(LightsPacket));
}

const std::vector<uint64_t> Server::game_players(const std::string &game_id) {
    std::vector<uint64_t> players;
    for (const auto &player : _badge_ips) {
        if (player.second.in_game() && player.second.current_game()->name() == game_id) {
            players.push_back(player.second.mac());
        }
    }

    return players;
}


void Server::handle_data(struct sockaddr_in &address, const char *data, ssize_t len) {
    if (len < sizeof(BasePacket)) {
        return;
    }

    switch (reinterpret_cast<const BasePacket*>(data)->type) {
        case PACKET_TYPE::STATUS: {
            const Status status = Status::decode_from_packet(reinterpret_cast<const StatusPacket*>(data));

            auto badge = _badge_ips.find((uint64_t)status.mac_address());
            if (badge == _badge_ips.end()) {
                // New badge!
                auto res = _badge_ips.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple((uint64_t)status.mac_address()),
                        std::forward_as_tuple(this, (uint64_t)status.mac_address(), address, sizeof(struct sockaddr), "", std::move(status))
                );

                badge = res.first;

                badge->second.set_lights(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            } else {
                // We don't want to do this for a new badge, since it has no last update
                // Check if the badge was rebooted
                if (status.update_count() < badge->second.last_status().update_count()) {
                    badge->second.set_lights(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                }
            }

            if (_status_callback) {
                _status_callback(status);
            }

            if (badge->second.in_game()) {
                if (badge->second.check_game_quit(status)) {
                    if (_leave_callback) {
                        _leave_callback(badge->first, badge->second.current_game()->name());
                    }

                    badge->second.set_lights(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                    badge->second.set_game(nullptr);
                }

                badge->second.set_last_status(std::move(status));
            } else {
                badge->second.set_last_status(std::move(status));
                for (const auto &game : _games) {
                    if (badge->second.check_game_join(&game)) {
                        badge->second.set_game(&game);

                        if (_join_callback) {
                            _join_callback(badge->first, game.name());
                        }
                        break;
                    }
                }
            }

            break;
        }

        case PACKET_TYPE::SCAN: {
            // TODO Scan packets may be entirely handled by another server
            /*
            const Scan scan = Scan::decode_from_packet(reinterpret_cast<const ScanPacket*>(data));
            std::cout << scan << std::endl;
            auto badge = _badge_ips.find((uint64_t)scan.mac_address());

            if (_scan_callback) {
                _scan_callback(scan);
            }

            if (badge != _badge_ips.end()) {
                badge->second.on_scan(scan);
            }*/

            break;
        }

        default:
            std::cout << "Got UNKNOWN packet!" << std::endl;
            // should never happen!
            break;
    }
}

void Server::run() {
    socklen_t clientlen = 0;
    struct sockaddr_in clientaddr{};
    char buf[BUFSIZE] {};

    _running = true;

    /*
     * socket: create the parent socket
     */
    _sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (_sockfd < 0) {
        std::cerr << "ERROR opening socket" << std::endl;
        return;
    }

    // Lets multiple apps bind to the same address simultaneously
    int optval = 1;
    setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR,
               (const void *) &optval, sizeof(int));

    struct sockaddr_in serveraddr{};
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short) PORT);

    if (bind(_sockfd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0) {
        std::cerr << "ERROR on binding" << std::endl;
    }


    ssize_t count;
    while (_running) {
        count = recvfrom(_sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (count < 0) {
            std::cerr << "ERROR in recvfrom" << std::endl;
            break;
        }

        /*
         * gethostbyaddr: determine who sent the datagram
         */
        char addr[NI_MAXHOST] {}, serv[NI_MAXSERV] {};
        if (!getnameinfo((sockaddr*) &clientaddr, clientlen,
                    addr, sizeof(addr),
                    serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV)) {

            handle_data(clientaddr, buf, count);
        } else {
            std::cerr << "ERR? " << gai_strerror(errno) << "(" << errno << ")" << std::endl;
        }
    }
}

void Server::send_packet(BadgeInfo *badge, const char *packet, size_t packet_len) {
    assert(_running);

    sendto(_sockfd, packet, packet_len,
           0,
           (struct sockaddr*)&badge->sock_address(),
           badge->sock_address_len());
}

void Server::send_packet(BadgeInfo &badge, const char *packet, size_t packet_len) {
    assert(_running);

    sendto(_sockfd, packet, packet_len,
           0,
           (struct sockaddr*)&badge.sock_address(),
           badge.sock_address_len());
}

void Server::send_packet(MacAddress &mac, const char *packet, size_t packet_len) {
    assert(_running);

    auto badge = _badge_ips.find((uint64_t)mac);
    if (badge != _badge_ips.end()) {
        send_packet(badge->second, packet, packet_len);
    }
}

void Server::send_packet(uint64_t mac, const char *packet, size_t packet_len) {
    assert(_running);

    auto badge = _badge_ips.find(mac);
    if (badge != _badge_ips.end()) {
        send_packet(badge->second, packet, packet_len);
    }
}

BadgeInfo *Server::find_badge(uint64_t mac) {
    auto badge = _badge_ips.find(mac);
    if (badge != _badge_ips.end()) {
        return &badge->second;
    }

    return nullptr;
}
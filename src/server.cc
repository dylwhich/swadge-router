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

void BadgeInfo::scan() {
    ScanRequestPacket packet;
    packet.base.type = SCAN_REQUEST;

    _server->send_packet(this, (char*)&packet, sizeof(ScanPacket));
}

void BadgeInfo::set_lights(uint8_t r1, uint8_t g1, uint8_t b1,
                           uint8_t r2, uint8_t g2, uint8_t b2,
                           uint8_t r3, uint8_t g3, uint8_t b3,
                           uint8_t r4, uint8_t g4, uint8_t b4,
                           uint8_t mask, uint8_t match) {
    LightsPacket packet;
    packet.base.type = LIGHTS;
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


void Server::handle_data(struct sockaddr_in &address, const char *data, ssize_t len) {
    if (len < sizeof(BasePacket)) {
        std::cout << "Discarding junk packet" << std::endl;
        return;
    }

    switch (reinterpret_cast<const BasePacket*>(data)->type) {
        case PACKET_TYPE::STATUS: {
            const Status status = Status::decode_from_packet(reinterpret_cast<const StatusPacket*>(data));
            std::cout << "[RECV]: " << status << std::endl;

            auto badge = _badge_ips.find((uint64_t)status.mac_address());
            if (badge != _badge_ips.end()) {
                badge->second.set_last_status(std::move(status));
                badge->second.set_lights(255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255);
            } else {
                _badge_ips.insert(std::make_pair((uint64_t)status.mac_address(),
                               BadgeInfo(this,
                                         address,
                                         sizeof(struct sockaddr),
                                         "",
                                         std::move(status))));
                //_badge_ips.find((uint64_t)status.mac_address())->second.scan();
            }

            break;
        }

        case PACKET_TYPE::SCAN: {
            const Scan scan = Scan::decode_from_packet(reinterpret_cast<const ScanPacket*>(data));
            std::cout << scan << std::endl;
            auto badge = _badge_ips.find((uint64_t)scan.mac_address());

            if (badge != _badge_ips.end()) {
                badge->second.on_scan(scan);
            }

            break;
        }

        default:
        std::cout << "Got UNKNOWN packet!" << std::endl;
            // should never happen!
            break;
    }
}

void Server::run() {
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    char buf[BUFSIZE];

    _running = true;

    std::cout << "Server running" << std::endl;

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

    struct sockaddr_in serveraddr;
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
        char addr[NI_MAXHOST], serv[NI_MAXSERV];
        if (!getnameinfo((sockaddr*) &clientaddr, clientlen,
                    addr, sizeof(addr),
                    serv, sizeof(serv),
                    NI_NUMERICHOST | NI_NUMERICSERV)) {

            std::cout << "server received " << count << " bytes from " << addr << std::endl;
            handle_data(clientaddr, buf, count);
        } else {
            std::cerr << "ERR? " << gai_strerror(errno) << "(" << errno << ")" << std::endl;
        }

        /*
         * sendto: echo the input back to the client
         */
        //n = sendto(_sockfd, buf, strlen(buf), 0, (struct sockaddr *) &clientaddr, clientlen);
        //if (n < 0) {
        //    error("ERROR in sendto");
        //    break;
        //}
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

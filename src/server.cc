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

void Server::handle_data(struct sockaddr_in &address, const char *data, ssize_t len) {
    if (len < sizeof(BasePacket)) {
        std::cout << "Discarding junk packet" << std::endl;
        return;
    }

    switch (reinterpret_cast<const BasePacket*>(data)->type) {
        case PACKET_TYPE::STATUS: {
            const Status status = Status::decode_from_packet(*reinterpret_cast<const StatusPacket*>(data));
            std::cout << "[RECV]: " << status << std::endl;

            auto badge = _badge_ips.find((uint64_t)status.mac_address());
            if (badge != _badge_ips.end()) {
                badge->second.set_last_status(std::move(status));
            } else {
                _badge_ips.insert(std::make_pair((uint64_t)status.mac_address(),
                               BadgeInfo(address,
                                         sizeof(struct sockaddr),
                                         "",
                                         std::move(status))));
            }

            break;
        }

        case PACKET_TYPE::SCAN: {
            std::cout << "Got SCAN packet" << std::endl;
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

void Server::send_packet(BadgeInfo &badge, const char *packet, size_t packet_len) {
    assert(_running);

    sendto(_sockfd, packet, packet_len,
           0,
           (sockaddr*)&badge.sock_address(),
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

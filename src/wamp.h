#ifndef SWADGE_WAMP_H
#define SWADGE_WAMP_H

#include <wampcc/wampcc.h>
#include <chrono>

#include "packets.h"
#include "server.h"

int64_t now();

class Wamp {
    std::shared_ptr<Server> _server;
    std::shared_ptr<wampcc::wamp_session> _session;

public:
    explicit Wamp(std::shared_ptr<Server> server)
            : _server(server),
              _session(nullptr) {}

    void on_scan(const Scan &scan);
    void on_status(const Status &status);
    void on_join(uint64_t badge_id, const std::string &game_name);
    void on_leave(uint64_t badge_id, const std::string &game_name);
    void on_new_badge(uint64_t badge_id);

    void on_subscribe_cb(wampcc::wamp_subscribed &evt);
    void on_lights(uint64_t badge_id,
                   int r1, int g1, int b1,
                   int r2, int g2, int b2,
                   int r3, int g3, int b3,
                   int r4, int g4, int b4,
                   int match=0, int mask=0);

    void run();
};

#endif

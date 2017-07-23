#ifndef WAMP_H
#define WAMP_H

#include <wampcc/wampcc.h>

#include "packets.h"
#include "server.h"

class Wamp {
    std::shared_ptr<Server> _server;
    std::shared_ptr<wampcc::wamp_session> _session;

public:
    explicit Wamp(std::shared_ptr<Server> server)
            : _server(server),
              _session(nullptr) {}

    void on_scan(const Scan &scan);
    void on_status(const Status &status);
    void run();
};

#endif

#include "server.h"
#include "wamp.h"

int main() {
    auto server = std::make_shared<Server>();

    std::thread server_thread(std::bind(&Server::run, server));

    Wamp wamp(server);
    std::thread wamp_thread(std::bind(&Wamp::run, wamp));

    server_thread.join();
    wamp_thread.join();
}
#ifndef WAMP_H
#define WAMP_H

#include <wampcc/wampcc.h>
#include <wampcc/kernel.h>
#include <iostream>
#include <jansson.h>

using namespace std::placeholders;

class Wamp {
public:
    Server &_server;
    std::shared_ptr<wampcc::wamp_session> _session;

    Wamp(Server &server)
            : _server(server),
              _session(nullptr) {}

    void on_scan(const Scan &scan) {
        wampcc::json_object data;
        data.emplace("timestamp", scan.timestamp());
        data.emplace("badge_id", (uint64_t)scan.mac_address());

        wampcc::json_array stations;
        for (const ScanStation &st : scan.stations()) {
            wampcc::json_object station;
            station.emplace("bssid", (const std::string&)st.mac());
            station.emplace("rssi", st.rssi());
            station.emplace("channel", st.channel());
            stations.push_back(std::move(station));
        }

        data.insert(std::make_pair("stations", stations));

        wampcc::wamp_args empty_args({{}});
        _session->publish("badge." + std::to_string((uint64_t)scan.mac_address()) + ".scan", data, empty_args);
    }

    void on_status(const Status &status) {
        if (status.last_button() != BUTTON::NONE) {
            wampcc::json_object btn_state;

            btn_state.emplace("badge_id", (uint64_t)status.mac_address());
            btn_state.emplace("button", status.last_button_name());
            _session->publish("badge." + std::to_string((uint64_t)status.mac_address()) + ".button." + (status.button_down() ? "press" : "release"), btn_state, wampcc::wamp_args({{}}));
        }
    }

    void run() {
        try {
            /* Create the wampcc kernel. */
            auto __logger = wampcc::logger::stream(wampcc::logger::lockable_cout,
                                                   wampcc::logger::levels_upto(wampcc::logger::eInfo), 1);
            wampcc::kernel the_kernel({}, __logger);

            /* Create the TCP socket and attempt to connect. */
            std::unique_ptr<wampcc::tcp_socket> socket(new wampcc::tcp_socket(&the_kernel));
            socket->connect("127.0.0.1", 8080).wait_for(std::chrono::seconds(3));

            if (!socket->is_connected())
                throw std::runtime_error("connect failed");

            /* With the connected socket, create a wamp session & logon to the realm
             * called 'default_realm'. */
            _session = wampcc::wamp_session::create<wampcc::websocket_protocol>(
                    &the_kernel, std::move(socket));

            wampcc::client_credentials credentials;
            credentials.realm = "swadges";
            credentials.authid = "router";
            credentials.authmethods = {"wampcra"};
            credentials.secret_fn = []() -> std::string { return "hunter2"; };


            _session->initiate_hello(credentials).wait_for(std::chrono::seconds(5));

            if (!_session->is_open()) {
                throw std::runtime_error("realm logon failed");
            }

            _server.set_on_scan(std::bind(&Wamp::on_scan, this, _1));

            _server.set_on_status(std::bind(&Wamp::on_status, this, _1));

            _session->provide("test", {},
                              [](wampcc::wamp_invocation &invocation) {
                                  invocation.yield({"it works?"});
                              });

            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }

        /*
        try {
            std::unique_ptr<wampcc::kernel> kernel = std::unique_ptr<wampcc::kernel>(
                    new wampcc::kernel({},
                                       wampcc::logger::stream(wampcc::logger::lockable_cout, wampcc::logger::levels_all(), true)));
            wampcc::wamp_router *router = new wampcc::wamp_router(kernel.get(), nullptr);

            std::cout << "Listening..." << std::endl;

            auto fut = router->listen(wampcc::auth_provider::no_auth_required(), 8080);

            std::cout << "1" << std::endl;

            if (auto ec = fut.get()) {
                throw std::runtime_error(ec.message());
            }
            std::cout << "2" << std::endl;

            std::future<void> endless_future;

            router->provide("crossbardemo", "test", {},
                           [](wampcc::wamp_invocation &invocation) {
                               invocation.yield({"it works?"});
                           });


            std::cout << "3" << std::endl;

            wampcc::wamp_args args({{"hello"}});
            while (true) {
                std::cout << "Publishing..." << std::endl;
                router->publish("crossbardemo", "test2", {}, std::move(args));
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }*/
    }
};

#endif

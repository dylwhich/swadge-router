#include "wamp.h"

using namespace std::placeholders;

void Wamp::on_scan(const Scan &scan){
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

    wampcc::wamp_args args{{{}}, data};
    _session->publish("badge." + std::to_string((uint64_t)scan.mac_address()) + ".scan", data, std::move(args));
}

void Wamp::on_status(const Status &status) {
    if (status.last_button() != BUTTON::NONE) {
        wampcc::wamp_args args{{status.last_button_name()}, {{"badge_id", (uint64_t)status.mac_address()}}};

        _session->publish("badge." + std::to_string((uint64_t)status.mac_address()) + ".button." + (status.button_down() ? "press" : "release"), {}, std::move(args));
    }
}

void Wamp::run() {
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

        _server->set_on_scan(std::bind(&Wamp::on_scan, this, _1));
        _server->set_on_status(std::bind(&Wamp::on_status, this, _1));

        std::cout << "Set on status" << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}

#include "wamp.h"

#include <regex>

using namespace std::placeholders;


int64_t now() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}


void Wamp::on_scan(const Scan &scan) {
    wampcc::json_object data;
    data.emplace("timestamp", scan.timestamp());
    data.emplace("badge_id", (uint64_t)scan.mac_address());

    wampcc::json_array stations;
    for (const ScanStation &st : scan.stations()) {
        stations.emplace_back(wampcc::json_object({{"bssid", (const std::string&)st.mac()},
                                                   {"rssi", st.rssi()},
                                                   {"channel", st.channel()}}));
    }

    data.insert(std::make_pair("stations", stations));

    wampcc::wamp_args args{{}, data};
    _session->publish("badge." + std::to_string((uint64_t)scan.mac_address()) + ".scan", data, std::move(args));
}

void Wamp::on_status(const Status &status) {
    if (status.last_button() != BUTTON::NONE) {
        wampcc::wamp_args args{{status.last_button_name()}, {
                {"badge_id", (uint64_t)status.mac_address()},
                {"timestamp", now()}}};

        _session->publish("badge." + std::to_string((uint64_t)status.mac_address()) + ".button." + (status.button_down() ? "press" : "release"), {}, std::move(args));
    }
}

void Wamp::on_join(uint64_t badge_id, const std::string &game_name) {
    _session->publish("game." + game_name + ".player.join", {}, {{badge_id}, {}});
}

void Wamp::on_leave(uint64_t badge_id, const std::string &game_name) {
    _session->publish("game." + game_name + ".player.leave", {}, {{badge_id}, {}});
}

void Wamp::on_subscribe_cb(wampcc::wamp_subscribed &evt) {
    if (evt.was_error) {
        std::cout << "Err: " << evt.error_uri << std::endl;
    } else {
        //std::cout << "Subscribe OK" << std::endl;
    }
}

void Wamp::on_lights(uint64_t badge_id,
                     int r1, int g1, int b1,
                     int r2, int g2, int b2,
                     int r3, int g3, int b3,
                     int r4, int g4, int b4,
                     int match, int mask) {

    _server->try_badge_call(&BadgeInfo::set_lights, badge_id, r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4, match, mask);
}

static const std::regex badge_id_regex("badge\\.([0-9]+)\\..*");

void Wamp::run() {
    try {
        /* Create the wampcc kernel. */
        auto __logger = wampcc::logger::stream(wampcc::logger::lockable_cout,
                                               wampcc::logger::levels_upto(wampcc::logger::eInfo), 1);
        wampcc::kernel the_kernel({}, __logger);

        /* Create the TCP socket and attempt to connect. */
        std::unique_ptr<wampcc::tcp_socket> socket(new wampcc::tcp_socket(&the_kernel));
        auto conn_fut = socket->connect("127.0.0.1", 1337);
        conn_fut.wait_for(std::chrono::seconds(3));

        if (!socket->is_connected()) {
            wampcc::uverr err = conn_fut.get();
            std::cout << "Connect failed: " << err.message() << std::endl;
            throw std::runtime_error("connect failed");
        }

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

        _session->subscribe("badge..lights_static", {{"match", "wildcard"}},
                            std::bind(&Wamp::on_subscribe_cb, this, _1),
                            [this] (wampcc::wamp_subscription_event ev) {
                                auto a = ev.args.args_list;

                                std::smatch res;
                                if (std::regex_match(ev.details["topic"].as_string(), res, badge_id_regex)) {
                                    uint64_t badge_id = std::stoull(res[1]);
                                    int a0 = a[0].as_int();
                                    int a1 = a[1].as_int();
                                    int a2 = a[2].as_int();
                                    int a3 = a[3].as_int();
                                    on_lights(badge_id,
                                              (a0 >> 16) & 0xff, (a0 >> 8) & 0xff, a0 & 0xff,
                                              (a1 >> 16) & 0xff, (a1 >> 8) & 0xff, a1 & 0xff,
                                              (a2 >> 16) & 0xff, (a2 >> 8) & 0xff, a2 & 0xff,
                                              (a3 >> 16) & 0xff, (a3 >> 8) & 0xff, a3 & 0xff,
                                              0, 0);
                                }
                            });

        _session->subscribe("game.kick", {},
                            std::bind(&Wamp::on_subscribe_cb, this, _1),
                            [this] (wampcc::wamp_subscription_event ev) {
                                auto args = ev.args.args_dict;

                                auto game_it = args.find("game_id");
                                if (game_it == args.end()) {
                                    std::cout << "Game ID not provided for kick" << std::endl;
                                    return;
                                }

                                auto badge_id_it = args.find("badge_id");
                                if (badge_id_it == args.end()) {
                                    std::cout << "Badge ID not provided for kick" << std::endl;
                                    return;
                                }

                                auto badge_it = _server->find_badge(badge_id_it->second.as_uint());
                                if (badge_it != nullptr) {
                                    badge_it->set_game(nullptr);
                                    _session->publish("game." + game_it->second.as_string() + ".player.leave", {}, {{badge_it->mac()}, {}});
                                    _server->try_badge_call(&BadgeInfo::set_lights, badge_it->mac(), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                                }

                            });

        _session->provide("game.register", {}, [](wampcc::wamp_invocation &invoc) {
            auto *server = reinterpret_cast<Server*>(invoc.user);
            auto args = invoc.args.args_list;
            auto kwargs = invoc.args.args_dict;

            std::string game_id, sequence, location;

            if (args.empty()) {
                // Maybe game_id is in the kwargs

                auto f = kwargs.find("game_id");
                if (f != kwargs.end()) {
                    game_id = f->second.as_string();
                } else {
                    // No game found... err
                    invoc.yield(wampcc::json_object {{"error", "game_id not present"}});
                }
            } else {
                game_id = args[0].as_string();
            }

            auto fseq = kwargs.find("sequence");
            if (fseq != kwargs.end()) {
                sequence = fseq->second.as_string();
            } else {
                sequence = "";
            }

            auto floc = kwargs.find("sequence");
            if (fseq != kwargs.end()) {
                location = floc->second.as_string();
            } else {
                location = "";
            }

            try {
                server->new_game(game_id, sequence, location);
                auto players = server->game_players(game_id);
                wampcc::json_array json_players;

                for (uint64_t player_id : players) {
                    json_players.emplace_back(player_id);
                }
                invoc.yield(wampcc::json_object {{"success", "Game successfully registered"}, {"players", json_players}});
            } catch (std::exception &e) {
                invoc.yield(wampcc::json_object {{"error", e.what()}});
            }
        }, _server.get());

        _server->set_on_scan(std::bind(&Wamp::on_scan, this, _1));
        _server->set_on_status(std::bind(&Wamp::on_status, this, _1));
        _server->set_on_join(std::bind(&Wamp::on_join, this, _1, _2));
        _server->set_on_leave(std::bind(&Wamp::on_leave, this, _1, _2));

        _session->publish("game.request_register", {}, {});

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}

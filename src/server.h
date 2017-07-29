#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>
#include <functional>
#include <unordered_map>
#include <stdexcept>

#include "packets.h"

class Server;

class GameInfo {
    std::string _name;
    std::string _sequence;
    std::string _location;

public:
    GameInfo(const std::string name, const std::string &sequence = "", const std::string &location = "")
            : _name(std::move(name)),
              _sequence(sequence),
              _location(location) {}

    const std::string &name() const {
        return _name;
    }

    bool use_sequence() const {
        return !_sequence.empty();
    }

    bool use_location() const {
        return !_location.empty();
    }

    const std::string &sequence() const {
        return _sequence;
    }

    void set_sequence(const std::string sequence) {
        _sequence = sequence;
    }

    const std::string &location() const {
        return _location;
    }

    void set_location(const std::string location) {
        _location = location;
    }
};

template<int len = 16>
class ButtonHistory {
    BUTTON _values[len];
    BUTTON *_cur, *_end;

public:
    ButtonHistory()
            : _values{BUTTON::NONE} {
        _cur = _values;
        _end = _values + len;
    }

    void record(BUTTON b) {
        *(_cur++) = b;
        if (_cur >= _end) _cur = _values;
    }

    /**
     * Matches the join code in any position
     * @param seq
     * @return
     */
    bool match_any(const char *seq) {
        // Start the pointer into the sequence at one past the last one we inserted
        const BUTTON *cmp = _cur;

        // start the pointer into the check-sequence at the last one
        const char *seq_start = seq + strlen(seq) - 1;

        // the pointer we will use to compare
        const char *idx = seq_start;

        const BUTTON *match_start = nullptr;

        for(;;) {
            // Move to the previous button that was pressed
            cmp--;

            // wrap around if we end up before the beginning
            if (cmp < _values) cmp = _end-1;

            // compare the buffer to the sequence, and move to the previous character if successful
            if (button_char(*cmp) == *idx) {
                // Keep track of the first position where we matched a character, because we need to backtrack
                // to the one after this if we fail in the middle
                if (match_start == nullptr) {
                    match_start = cmp;
                }

                idx--;
            } else {
                // Move back to the end of the sequence to start checking again
                idx = seq_start;

                // Backtrack to where we last succeeded and try the next one
                if (match_start != nullptr) {
                    cmp = match_start;
                    match_start = nullptr;
                }
            }

            // If we get all the way to the start of the sequence, then we've checked everywhere (successfully)
            if (idx < seq) {
                return true;
            }


            // If we get back to where we started, then we've checked everywhere (unsuccessfully)
            if (cmp == _cur) {
                return false;
            }
        }
    }

    /**
     * Matches the join code in the last position
     * @param seq
     * @return
     */
    bool match(const char *seq) {
        // Start the pointer into the sequence at one past the last one we inserted
        const BUTTON *cmp = _cur;

        // start the pointer into the check-sequence at the last one
        const char *seq_start = seq + strlen(seq) - 1;

        // the pointer we will use to compare
        const char *idx = seq_start;

        for(;;) {
            // Move to the previous button that was pressed
            cmp--;

            // wrap around if we end up before the beginning
            if (cmp < _values) cmp = _end-1;

            // compare the buffer to the sequence, and move to the previous character if successful
            if (button_char(*cmp) == *idx) {
                idx--;
            } else {
                return false;
            }

            // If we get all the way to the start of the sequence, then we've checked everywhere (successfully)
            if (idx < seq) {
                return true;
            }


            // If we get back to where we started, then the sequence is too long.
            if (cmp == _cur) {
                return false;
            }
        }
    }
};

class BadgeInfo {
    Server *_server;

    uint64_t _mac;

    struct sockaddr_in _sockaddr;
    socklen_t _sockaddr_len;

    std::string _host;

    Status _last_status;
    uint64_t _station;

    std::string _location;
    Scan _last_scan;
    ButtonHistory<12> _history;
    const GameInfo *_game;

public:
    BadgeInfo(Server *server,
              uint64_t mac,
              struct sockaddr_in &sockaddr,
              socklen_t sockaddr_len,
              const std::string &host,
              const Status &&status,
              uint64_t station = 0)
            : _server(server),
              _mac(mac),
              _sockaddr(sockaddr),
              _sockaddr_len(sockaddr_len),
              _host(host),
              _last_status(status),
              _station(station),
              _location(),
              _last_scan(),
              _history(),
              _game(nullptr) {}

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

        _station = (uint64_t)status.bssid();

        if (_last_status.last_button() != BUTTON::NONE && !_last_status.button_down()) {
            _history.record(_last_status.last_button());
        }
    }

    uint64_t station() { return _station; }

    void on_scan(const Scan &scan) {
        if (!_last_scan.update(scan)) {
            _last_scan = scan;
        }
    }

    const Scan &last_scan() {
        return _last_scan;
    }

    bool in_game() const {
        return _game != nullptr;
    }

    const GameInfo *current_game() const {
        return _game;
    }

    bool check_game_join(const GameInfo *game) {
        return (game->use_sequence() && _history.match(game->sequence().c_str()))
                || (game->use_location() && _location == game->location());
    }

    void set_game(const GameInfo *game) {
        _game = game;
    }

    uint64_t mac() const {
        return _mac;
    }

    void scan();
    void set_lights(uint8_t r1, uint8_t g1, uint8_t b1,
                    uint8_t r2, uint8_t g2, uint8_t b2,
                    uint8_t r3, uint8_t g3, uint8_t b3,
                    uint8_t r4, uint8_t g4, uint8_t b4,
                    uint8_t mask = 0, uint8_t match = 0);
};

using ScanCallback = std::function<void(const Scan&)>;
using StatusCallback = std::function<void(const Status&)>;
using JoinCallback = std::function<void(uint64_t, const std::string&)>;


class Server {
    int _sockfd;
    bool _running;

    ScanCallback _scan_callback;
    StatusCallback _status_callback;
    JoinCallback _join_callback;


    std::unordered_map<uint64_t, BadgeInfo> _badge_ips;
    std::vector<GameInfo> _games;

public:
    Server()
            : _sockfd(-1),
              _running(false) {}

    void set_on_scan(ScanCallback cb) {
        _scan_callback = cb;
    }

    void set_on_status(StatusCallback cb) {
        _status_callback = cb;
    }

    void set_on_join(JoinCallback cb) {
        _join_callback = cb;
    }

    void new_game(const std::string &name, const std::string &sequence = "", const std::string &location = "") {
        GameInfo *found_game = nullptr;

        for (auto &game : _games) {
            if (game.name() == name) {
                found_game = &game;
            } else {
                if (game.use_sequence() && game.sequence() == sequence) {
                    throw "Sequence " + sequence + " already in use by " + game.name();
                }

                if (game.use_location() && game.location() == location) {
                    throw "Location " + location + " already in use by " + game.name();
                }
            }
        }

        if (found_game != nullptr) {
            found_game->set_sequence(sequence);
            found_game->set_location(location);
        } else {
            _games.emplace_back(name, sequence, location);
        }
    }

    const std::vector<uint64_t> game_players(const std::string &name);

    void handle_data(struct sockaddr_in &address, const char *data, ssize_t len);

    void send_packet(BadgeInfo *badge, const char *packet, size_t packet_len);
    void send_packet(BadgeInfo &badge, const char *packet, size_t packet_len);
    void send_packet(MacAddress &mac, const char *packet, size_t packet_len);
    void send_packet(uint64_t mac, const char *packet, size_t packet_len);

    BadgeInfo *find_badge(uint64_t mac);

    template<typename M, typename... Args>
    void try_badge_call(M m, uint64_t mac, Args&&... args) {
        auto badge = find_badge(mac);
        if (badge != nullptr) {
            std::bind(m, badge, std::forward<Args>(args)...)();
        }
    }

    void run();
};

#endif


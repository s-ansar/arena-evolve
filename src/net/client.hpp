#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "net/protocol.hpp"

namespace net {

// Decides a command from the latest state. me is this client's robot index.
using Controller = std::function<Command(const State& state, std::uint8_t me)>;

// What one client saw of a match.
struct MatchReport {
    std::uint8_t robot = 0;   // which robot this client drove
    std::uint32_t ticks = 0;  // tick number of the last State received
    // True if the last State showed the match decided (at most one robot in the ring).
    // False means the connection ended mid-match: a dropped game (or a timeout draw,
    // which the evolved and chase bots never produce).
    bool finished = false;
};

// Connect to a server, play one match and report on it once the server disconnects. After
// every State message, the controller's command is sent back. An empty controller sends
// nothing (a robot that just sits there).
// Throws on connection failure, or if the server breaks the protocol or closes the
// connection before the match starts (no Welcome).
MatchReport play_match(const std::string& host, std::uint16_t port, const Controller& controller);

}  // namespace net

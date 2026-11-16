#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "net/protocol.hpp"

namespace net {

// Decides a command from the latest state. me is this client's robot index.
using Controller = std::function<Command(const State& state, std::uint8_t me)>;

// Connect to a server, play one match and return this client's robot index once the
// server disconnects. After every State message, the controller's command is sent back.
// An empty controller sends nothing (a robot that just sits there).
// Throws on connection failure or if the server breaks the protocol.
std::uint8_t play_match(const std::string& host, std::uint16_t port, const Controller& controller);

}  // namespace net

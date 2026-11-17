#include "net/client.hpp"

#include <optional>
#include <vector>

#include "net/socket.hpp"

namespace net {

namespace {

// A match is over when at most one robot is left in the ring (the same rule the server uses).
bool shows_match_over(const State& state) {
    int in_ring = 0;
    for (const RobotState& robot : state.robots) {
        in_ring += robot.in_ring ? 1 : 0;
    }
    return in_ring <= 1;
}

}  // namespace

MatchReport play_match(const std::string& host, std::uint16_t port, const Controller& controller) {
    Socket socket = connect_tcp(host, port);
    std::vector<std::uint8_t> buffer;
    std::optional<std::uint8_t> me;  // unknown until the Welcome arrives
    MatchReport report;

    // One thread is enough here: the client only ever reacts to the server.
    while (receive_some(socket, buffer)) {
        while (std::optional<Message> message = try_decode(buffer)) {
            if (const Welcome* welcome = std::get_if<Welcome>(&*message)) {
                me = welcome->robot_id;
            } else if (const State* state = std::get_if<State>(&*message)) {
                if (!me) {
                    throw ProtocolError("State arrived before Welcome");
                }
                report.ticks = state->tick;
                report.finished = shows_match_over(*state);
                if (controller && !send_all(socket, encode(controller(*state, *me)))) {
                    break;  // server gone; the report says how far the match got
                }
            }
        }
    }
    if (!me) {
        throw ProtocolError("server closed the connection before sending Welcome");
    }
    report.robot = *me;
    return report;
}

}  // namespace net

#include "net/client.hpp"

#include <optional>
#include <vector>

#include "net/socket.hpp"

namespace net {

std::uint8_t play_match(const std::string& host, std::uint16_t port, const Controller& controller) {
    Socket socket = connect_tcp(host, port);
    std::vector<std::uint8_t> buffer;
    std::optional<std::uint8_t> me;  // unknown until the Welcome arrives

    // One thread is enough here: the client only ever reacts to the server.
    while (receive_some(socket, buffer)) {
        while (std::optional<Message> message = try_decode(buffer)) {
            if (const Welcome* welcome = std::get_if<Welcome>(&*message)) {
                me = welcome->robot_id;
            } else if (const State* state = std::get_if<State>(&*message)) {
                if (!me) {
                    throw ProtocolError("State arrived before Welcome");
                }
                if (controller && !send_all(socket, encode(controller(*state, *me)))) {
                    return *me;  // server gone
                }
            }
        }
    }
    if (!me) {
        throw ProtocolError("server closed the connection before sending Welcome");
    }
    return *me;
}

}  // namespace net

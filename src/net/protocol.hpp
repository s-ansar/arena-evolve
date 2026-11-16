#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

// The binary protocol between the server and controller clients, sent over TCP.
//
// Every message is a frame:
//   byte 0      message type (see MessageType)
//   bytes 1-2   payload length in bytes, unsigned, big-endian (network byte order)
//   bytes 3...  payload
//
// TCP is a byte stream, not a stream of messages: one recv() can return half a frame or
// three frames at once. The length field is what lets the receiver find frame boundaries.
//
// All numbers are big-endian. Floats are 32-bit IEEE 754, sent as their raw bits.
// This file only turns messages into bytes and back. It does no I/O, so it is portable
// and easy to test (and to fuzz later).
namespace net {

enum class MessageType : std::uint8_t {
    Welcome = 1,  // server -> client, once, right after connecting
    Command = 2,  // client -> server, any time
    State = 3,    // server -> client, after every tick (TCP to controllers, UDP to watchers)
    Watch = 4,    // watcher -> server over UDP: "send me states for the next few seconds"
};

// Tells a new client which robot it controls (an index into State::robots).
// Payload: robot_id (u8). 1 byte.
struct Welcome {
    std::uint8_t robot_id = 0;
    bool operator==(const Welcome&) const = default;
};

// The thrust the client wants for its robot, in metres per second squared.
// Payload: thrust_x (f32), thrust_y (f32). 8 bytes. NaN and infinity are rejected.
struct Command {
    float thrust_x = 0.0f;
    float thrust_y = 0.0f;
    bool operator==(const Command&) const = default;
};

// One robot as seen in a State message. 17 bytes: x, y, vx, vy (f32 each), in_ring (u8, 0 or 1).
struct RobotState {
    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    bool in_ring = true;
    bool operator==(const RobotState&) const = default;
};

// The whole arena after one tick.
// Payload: tick (u32), sent_at_ns (u64), robot count (u8), then one RobotState per robot.
// 13 + 17 * count bytes.
struct State {
    std::uint32_t tick = 0;
    // The server's steady_clock reading when it sent this, in nanoseconds. steady_clock on
    // Linux is the system-wide monotonic clock, so a receiver on the SAME machine can
    // subtract to get the one-way latency. Across machines the numbers mean nothing.
    std::uint64_t sent_at_ns = 0;
    std::vector<RobotState> robots;
    bool operator==(const State&) const = default;
};

// A watcher's request to receive State datagrams. Empty payload. Watchers resend it every
// second; the server forgets a watcher it has not heard from for a few seconds, because
// UDP has no connection that closes when the watcher goes away.
struct Watch {
    bool operator==(const Watch&) const = default;
};

using Message = std::variant<Welcome, Command, State, Watch>;

constexpr std::size_t kHeaderSize = 3;
// Upper limit on payload size. A receiver rejects anything bigger straight from the header,
// so a bad or hostile peer cannot make it wait for (and buffer) a huge message.
constexpr std::size_t kMaxPayload = 4096;

// Thrown when the bytes cannot be a valid message. The connection should then be closed:
// after a bad frame there is no safe way to find where the next one starts.
class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Turn a message into a complete frame (header + payload), ready to send.
std::vector<std::uint8_t> encode(const Message& message);

// Try to take one complete message from the front of buffer.
//   - A whole frame is there: it is removed from buffer and returned.
//   - Not enough bytes yet: returns nothing and leaves buffer unchanged (wait for more).
//   - The bytes are invalid: throws ProtocolError.
std::optional<Message> try_decode(std::vector<std::uint8_t>& buffer);

}  // namespace net

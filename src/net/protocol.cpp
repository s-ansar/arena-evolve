#include "net/protocol.hpp"

#include <bit>
#include <cmath>
#include <span>

namespace net {

namespace {

static_assert(sizeof(float) == 4, "the protocol sends floats as 32 bits");

// Appends numbers to a byte vector, most significant byte first (big-endian).
// Shifting bytes out by hand gives the same result on any CPU, so no htonl() is needed.
class Writer {
public:
    void u8(std::uint8_t value) { bytes_.push_back(value); }
    void u16(std::uint16_t value) {
        u8(static_cast<std::uint8_t>(value >> 8));
        u8(static_cast<std::uint8_t>(value));
    }
    void u32(std::uint32_t value) {
        u16(static_cast<std::uint16_t>(value >> 16));
        u16(static_cast<std::uint16_t>(value));
    }
    void u64(std::uint64_t value) {
        u32(static_cast<std::uint32_t>(value >> 32));
        u32(static_cast<std::uint32_t>(value));
    }
    // std::bit_cast copies the float's raw bits into an integer without changing them.
    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }

    std::vector<std::uint8_t>& bytes() { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

// Reads numbers back from a payload. Every read checks there are enough bytes left, so a
// short or truncated payload throws instead of reading past the end of the buffer.
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}

    std::uint8_t u8() {
        need(1);
        return data_[pos_++];
    }
    std::uint16_t u16() {
        const std::uint16_t high = u8();
        return static_cast<std::uint16_t>((high << 8) | u8());
    }
    std::uint32_t u32() {
        const std::uint32_t high = u16();
        return (high << 16) | u16();
    }
    std::uint64_t u64() {
        const std::uint64_t high = u32();
        return (high << 32) | u32();
    }
    float f32() { return std::bit_cast<float>(u32()); }

    // A finite float: NaN or infinity from a client would poison the physics for everyone.
    float finite_f32() {
        const float value = f32();
        if (!std::isfinite(value)) {
            throw ProtocolError("number is NaN or infinite");
        }
        return value;
    }

    bool finished() const { return pos_ == data_.size(); }

private:
    void need(std::size_t count) const {
        if (data_.size() - pos_ < count) {
            throw ProtocolError("payload too short");
        }
    }

    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

// One overload per message type. std::visit picks the right one for the variant.
struct PayloadEncoder {
    Writer& out;

    std::uint8_t operator()(const Welcome& m) const {
        out.u8(m.robot_id);
        return static_cast<std::uint8_t>(MessageType::Welcome);
    }
    std::uint8_t operator()(const Command& m) const {
        out.f32(m.thrust_x);
        out.f32(m.thrust_y);
        return static_cast<std::uint8_t>(MessageType::Command);
    }
    std::uint8_t operator()(const State& m) const {
        if (m.robots.size() > 255) {
            throw ProtocolError("too many robots for one State message");
        }
        out.u32(m.tick);
        out.u64(m.sent_at_ns);
        out.u8(static_cast<std::uint8_t>(m.robots.size()));
        for (const RobotState& r : m.robots) {
            out.f32(r.x);
            out.f32(r.y);
            out.f32(r.vx);
            out.f32(r.vy);
            out.u8(r.in_ring ? 1 : 0);
        }
        return static_cast<std::uint8_t>(MessageType::State);
    }
    std::uint8_t operator()(const Watch&) const {
        return static_cast<std::uint8_t>(MessageType::Watch);  // no payload
    }
};

Message decode_payload(std::uint8_t type, Reader& in) {
    switch (static_cast<MessageType>(type)) {
    case MessageType::Welcome:
        return Welcome{in.u8()};
    case MessageType::Command: {
        Command m;
        m.thrust_x = in.finite_f32();
        m.thrust_y = in.finite_f32();
        return m;
    }
    case MessageType::State: {
        State m;
        m.tick = in.u32();
        m.sent_at_ns = in.u64();
        const std::uint8_t count = in.u8();
        for (std::uint8_t i = 0; i < count; ++i) {
            RobotState r;
            r.x = in.finite_f32();
            r.y = in.finite_f32();
            r.vx = in.finite_f32();
            r.vy = in.finite_f32();
            const std::uint8_t flag = in.u8();
            if (flag > 1) {
                throw ProtocolError("in_ring must be 0 or 1");
            }
            r.in_ring = flag == 1;
            m.robots.push_back(r);
        }
        return m;
    }
    case MessageType::Watch:
        return Watch{};
    }
    throw ProtocolError("unknown message type");
}

}  // namespace

std::vector<std::uint8_t> encode(const Message& message) {
    Writer payload;
    const std::uint8_t type = std::visit(PayloadEncoder{payload}, message);
    if (payload.bytes().size() > kMaxPayload) {
        throw ProtocolError("payload too long");
    }

    Writer frame;
    frame.u8(type);
    frame.u16(static_cast<std::uint16_t>(payload.bytes().size()));
    frame.bytes().insert(frame.bytes().end(), payload.bytes().begin(), payload.bytes().end());
    return std::move(frame.bytes());
}

std::optional<Message> try_decode(std::vector<std::uint8_t>& buffer) {
    if (buffer.size() < kHeaderSize) {
        return std::nullopt;
    }
    const std::uint8_t type = buffer[0];
    const std::size_t length = (static_cast<std::size_t>(buffer[1]) << 8) | buffer[2];

    // Check the header before waiting for the payload, so a bad frame is rejected at once.
    if (type < static_cast<std::uint8_t>(MessageType::Welcome) ||
        type > static_cast<std::uint8_t>(MessageType::Watch)) {
        throw ProtocolError("unknown message type");
    }
    if (length > kMaxPayload) {
        throw ProtocolError("payload too long");
    }
    if (buffer.size() < kHeaderSize + length) {
        return std::nullopt;  // the rest of the frame has not arrived yet
    }

    Reader in(std::span<const std::uint8_t>(buffer).subspan(kHeaderSize, length));
    Message message = decode_payload(type, in);
    if (!in.finished()) {
        throw ProtocolError("payload longer than its message needs");
    }

    // Remove the frame from the front. Erasing from the front of a vector copies the rest
    // down, but the buffer only ever holds a few small frames, so this is cheap enough.
    buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(kHeaderSize + length));
    return message;
}

}  // namespace net

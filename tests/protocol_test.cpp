#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "net/protocol.hpp"

using net::Command;
using net::Message;
using net::ProtocolError;
using net::State;
using net::Welcome;
using Bytes = std::vector<std::uint8_t>;

namespace {

Message round_trip(const Message& message) {
    Bytes buffer = net::encode(message);
    std::optional<Message> decoded = net::try_decode(buffer);
    EXPECT_TRUE(buffer.empty());  // the whole frame was used up
    return decoded.value();
}

}  // namespace

TEST(Protocol, WelcomeHasTheDocumentedBytes) {
    // type 1, length 0x0001, robot id 7
    EXPECT_EQ(net::encode(Welcome{7}), (Bytes{1, 0, 1, 7}));
}

TEST(Protocol, CommandFloatsAreBigEndian) {
    // 1.0f is 0x3F800000 in IEEE 754; -2.0f is 0xC0000000.
    EXPECT_EQ(net::encode(Command{1.0f, -2.0f}),
              (Bytes{2, 0, 8, 0x3F, 0x80, 0x00, 0x00, 0xC0, 0x00, 0x00, 0x00}));
}

TEST(Protocol, EveryMessageTypeRoundTrips) {
    EXPECT_EQ(round_trip(Welcome{3}), Message{Welcome{3}});
    EXPECT_EQ(round_trip(Command{0.25f, -1.5f}), Message{(Command{0.25f, -1.5f})});

    State state;
    state.tick = 123456;
    state.robots = {{0.5f, -0.5f, 1.0f, 0.0f, true}, {-0.9f, 0.1f, 0.0f, -0.25f, false}};
    EXPECT_EQ(round_trip(state), Message{state});
}

TEST(Protocol, WaitsForTheRestOfAFrame) {
    const Bytes frame = net::encode(Command{1.0f, 2.0f});
    Bytes buffer;
    // Feed one byte at a time, like a slow network. Nothing decodes until the last byte.
    for (std::size_t i = 0; i + 1 < frame.size(); ++i) {
        buffer.push_back(frame[i]);
        EXPECT_FALSE(net::try_decode(buffer).has_value());
        EXPECT_EQ(buffer.size(), i + 1);  // nothing removed while incomplete
    }
    buffer.push_back(frame.back());
    EXPECT_EQ(net::try_decode(buffer), Message{(Command{1.0f, 2.0f})});
}

TEST(Protocol, DecodesTwoFramesThatArriveTogether) {
    Bytes buffer = net::encode(Welcome{1});
    const Bytes second = net::encode(Command{3.0f, 4.0f});
    buffer.insert(buffer.end(), second.begin(), second.end());

    EXPECT_EQ(net::try_decode(buffer), Message{Welcome{1}});
    EXPECT_EQ(net::try_decode(buffer), Message{(Command{3.0f, 4.0f})});
    EXPECT_TRUE(buffer.empty());
}

TEST(Protocol, RejectsUnknownType) {
    Bytes buffer{99, 0, 0};
    EXPECT_THROW(net::try_decode(buffer), ProtocolError);
}

TEST(Protocol, RejectsHugeLengthFromTheHeaderAlone) {
    Bytes buffer{2, 0xFF, 0xFF};  // claims 65535 bytes; we must not wait for them
    EXPECT_THROW(net::try_decode(buffer), ProtocolError);
}

TEST(Protocol, RejectsWrongPayloadSize) {
    Bytes too_short{2, 0, 4, 0, 0, 0, 0};  // a Command needs 8 bytes
    EXPECT_THROW(net::try_decode(too_short), ProtocolError);

    Bytes too_long{1, 0, 2, 7, 7};  // a Welcome needs 1 byte
    EXPECT_THROW(net::try_decode(too_long), ProtocolError);
}

TEST(Protocol, RejectsNaNThrust) {
    Bytes buffer = net::encode(Command{std::numeric_limits<float>::quiet_NaN(), 0.0f});
    EXPECT_THROW(net::try_decode(buffer), ProtocolError);
}

TEST(Protocol, RejectsBadInRingFlag) {
    State state;
    state.robots = {{}};
    Bytes buffer = net::encode(state);
    buffer.back() = 2;  // in_ring must be 0 or 1
    EXPECT_THROW(net::try_decode(buffer), ProtocolError);
}

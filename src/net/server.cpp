#include "net/server.hpp"

#include <memory>
#include <thread>
#include <vector>

#include "net/mailbox.hpp"
#include "net/protocol.hpp"
#include "net/thread_safe_queue.hpp"
#include "sim/loop.hpp"

namespace net {

namespace {

using Bytes = std::vector<std::uint8_t>;

// A command tagged with the robot it is for. The reader thread knows which robot its
// client controls, so a client can never steer someone else's robot.
struct RobotCommand {
    std::size_t robot = 0;
    Command command;
};

// One connected client and its two threads.
class Connection {
public:
    Connection(Socket socket, std::size_t robot, ThreadSafeQueue<RobotCommand>& commands)
        : socket_(std::move(socket)),
          reader_([this, robot, &commands] { read_loop(robot, commands); }),
          writer_([this] { write_loop(); }) {}

    // Stop both threads and wait for them. Order matters:
    //   1. close the outbox, so the writer sends any final state and then exits;
    //   2. join the writer, so that final state really goes out before the socket stops;
    //   3. shut the socket down, which makes the reader's blocked recv() return;
    //   4. join the reader.
    // The Socket member is destroyed (closed) only after both threads have finished,
    // so no thread can ever use a closed or reused file descriptor.
    ~Connection() {
        outbox_.close();
        writer_.join();
        socket_.shutdown_both();
        reader_.join();
    }

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Called by the simulation thread. Never blocks (see Mailbox).
    void send_state(Bytes frame) { outbox_.put(std::move(frame)); }

private:
    void read_loop(std::size_t robot, ThreadSafeQueue<RobotCommand>& commands) {
        Bytes buffer;
        try {
            while (receive_some(socket_, buffer)) {
                while (std::optional<Message> message = try_decode(buffer)) {
                    if (const Command* command = std::get_if<Command>(&*message)) {
                        commands.push({robot, *command});
                    }
                    // Any other message type from a client is ignored.
                }
            }
        } catch (const ProtocolError&) {
            // Garbage from this client: stop reading it. Its robot just stops getting
            // new commands; the match goes on for everyone else.
        }
    }

    void write_loop() {
        while (std::optional<Bytes> frame = outbox_.take()) {
            if (!send_all(socket_, *frame)) {
                break;  // client gone
            }
        }
    }

    // Declared before the threads so it is constructed before they start using it.
    // Shared by both threads: reading and writing one socket from two threads at the
    // same time is allowed by POSIX (one reader, one writer).
    Socket socket_;
    Mailbox<Bytes> outbox_;  // sim thread puts, writer thread takes (Mailbox has its own lock)
    std::thread reader_;
    std::thread writer_;
};

// Limits on UDP watchers. A Watch datagram is tiny and each State back is bigger, and UDP
// source addresses can be faked, so an open server could be used to flood someone else.
// Capping the number of watchers and expiring silent ones limits that. Fine for a lab;
// a public server would need authentication.
constexpr std::size_t kMaxWatchers = 8;
constexpr auto kWatchLease = std::chrono::seconds{5};

State snapshot(const sim::Arena& arena, int tick) {
    State state;
    state.tick = static_cast<std::uint32_t>(tick);
    state.sent_at_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    for (const sim::Robot& r : arena.robots) {
        state.robots.push_back({static_cast<float>(r.position.x), static_cast<float>(r.position.y),
                                static_cast<float>(r.velocity.x), static_cast<float>(r.velocity.y),
                                r.in_ring});
    }
    return state;
}

}  // namespace

sim::Arena starting_arena() {
    sim::Arena arena;
    arena.robots.push_back(sim::Robot{.position = {-0.5, 0.0}});
    arena.robots.push_back(sim::Robot{.position = {0.5, 0.0}});
    return arena;
}

Server::Server(std::uint16_t port)
    : listener_(listen_tcp(port)), port_(local_port(listener_)), udp_(open_udp(port_)) {}

void Server::update_watchers() {
    const auto now = Clock::now();
    // Read every waiting datagram without blocking. Anything that is not a valid Watch
    // frame (exactly one frame, nothing left over) is ignored.
    while (std::optional<Datagram> datagram = receive_datagram(udp_, false)) {
        try {
            std::vector<std::uint8_t>& bytes = datagram->bytes;
            const std::optional<Message> message = try_decode(bytes);
            if (!message || !bytes.empty() || !std::holds_alternative<Watch>(*message)) {
                continue;
            }
        } catch (const ProtocolError&) {
            continue;
        }

        bool known = false;
        for (Watcher& watcher : watchers_) {
            if (watcher.endpoint == datagram->from) {
                watcher.last_heard = now;  // renew the lease
                known = true;
            }
        }
        if (!known && watchers_.size() < kMaxWatchers) {
            watchers_.push_back({datagram->from, now});
        }
    }
    // Forget watchers whose lease ran out (std::erase_if is C++20).
    std::erase_if(watchers_, [now](const Watcher& w) { return now - w.last_heard > kWatchLease; });
}

void Server::broadcast(const std::vector<std::uint8_t>& frame) {
    for (const Watcher& watcher : watchers_) {
        send_datagram(udp_, watcher.endpoint, frame);  // fire and forget: loss is allowed
    }
}

MatchResult Server::run_match(sim::Arena arena, sim::Seconds dt, int max_ticks) {
    // Declared before the connections, so it is destroyed after them: reader threads push
    // into it until their Connection is destroyed.
    ThreadSafeQueue<RobotCommand> commands;
    std::vector<std::unique_ptr<Connection>> connections;

    // Accept one client per robot. The Welcome goes out before the client's threads start,
    // so it is always the first message the client sees.
    for (std::size_t robot = 0; robot < arena.robots.size(); ++robot) {
        Socket client = accept_client(listener_);
        // A client that stops reading for a second is treated as gone, so its writer thread
        // cannot hang and stop the match from shutting down.
        set_send_timeout(client, std::chrono::seconds{1});
        send_all(client, encode(Welcome{static_cast<std::uint8_t>(robot)}));
        connections.push_back(std::make_unique<Connection>(std::move(client), robot, commands));
    }

    constexpr auto relaxed = std::memory_order_relaxed;  // see Metrics for why this is enough
    metrics_.match_running.store(true, relaxed);

    // The tick hook runs on this (the simulation) thread after every step.
    MatchResult result;
    result.stats = sim::run_realtime(arena, dt, max_ticks, [&](sim::Arena& a, int tick) {
        const Bytes frame = encode(snapshot(a, tick));
        for (const auto& connection : connections) {
            connection->send_state(frame);
        }
        update_watchers();
        broadcast(frame);
        metrics_.ticks.fetch_add(1, relaxed);
        metrics_.watchers.store(watchers_.size(), relaxed);
        // Apply every command that arrived since the last tick. If one client sent several,
        // the last one wins. The new thrust takes effect from the next step.
        while (std::optional<RobotCommand> rc = commands.try_pop()) {
            sim::Robot& robot = a.robots[rc->robot];
            robot.thrust = {rc->command.thrust_x, rc->command.thrust_y};
        }
    });

    // Destroying the connections sends each client the final state, then disconnects it.
    connections.clear();
    result.winner = sim::winner(arena);

    const auto seconds = [](std::chrono::nanoseconds d) { return std::chrono::duration<double>(d).count(); };
    (result.winner ? metrics_.matches_won : metrics_.matches_drawn).fetch_add(1, relaxed);
    metrics_.late_ticks.fetch_add(static_cast<std::uint64_t>(result.stats.late_ticks), relaxed);
    metrics_.last_interval_mean_seconds.store(seconds(result.stats.interval.mean()), relaxed);
    metrics_.last_jitter_seconds.store(seconds(result.stats.interval.stddev()), relaxed);
    metrics_.last_lateness_max_seconds.store(seconds(result.stats.lateness.max()), relaxed);
    metrics_.match_running.store(false, relaxed);
    return result;
}

}  // namespace net

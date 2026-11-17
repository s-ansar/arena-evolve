#include "net/metrics.hpp"

#include <sstream>

namespace net {

namespace {

template <typename T>
void metric(std::ostringstream& out, const char* name, const char* type, const char* help, T value) {
    out << "# HELP " << name << ' ' << help << '\n'
        << "# TYPE " << name << ' ' << type << '\n'
        << name << ' ' << value << '\n';
}

std::string response(const char* status, const char* content_type, const std::string& body) {
    // Connection: close keeps the server simple: one request per connection.
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return out.str();
}

}  // namespace

std::string render_prometheus(const Metrics& m) {
    constexpr auto relaxed = std::memory_order_relaxed;
    std::ostringstream out;
    out.precision(9);
    // One counter with a label is the Prometheus way to split a count by kind.
    out << "# HELP arena_matches_total Matches finished, by result.\n"
        << "# TYPE arena_matches_total counter\n"
        << "arena_matches_total{result=\"win\"} " << m.matches_won.load(relaxed) << '\n'
        << "arena_matches_total{result=\"draw\"} " << m.matches_drawn.load(relaxed) << '\n';
    metric(out, "arena_matches_started_total", "counter", "Matches started (all players connected).",
           m.matches_started.load(relaxed));
    metric(out, "arena_ticks_total", "counter", "Simulation ticks run.", m.ticks.load(relaxed));
    metric(out, "arena_late_ticks_total", "counter", "Ticks that started after their deadline.",
           m.late_ticks.load(relaxed));
    metric(out, "arena_watchers", "gauge", "UDP watchers currently registered.", m.watchers.load(relaxed));
    metric(out, "arena_match_running", "gauge", "1 while a match is being played.",
           m.match_running.load(relaxed) ? 1 : 0);
    metric(out, "arena_draining", "gauge", "1 once a graceful stop was requested.",
           m.draining.load(relaxed) ? 1 : 0);
    metric(out, "arena_last_match_tick_interval_mean_seconds", "gauge",
           "Mean time between tick starts in the last match.", m.last_interval_mean_seconds.load(relaxed));
    metric(out, "arena_last_match_tick_jitter_seconds", "gauge",
           "Standard deviation of the tick interval in the last match.", m.last_jitter_seconds.load(relaxed));
    metric(out, "arena_last_match_lateness_max_seconds", "gauge",
           "Worst wake-up lateness in the last match.", m.last_lateness_max_seconds.load(relaxed));
    return out.str();
}

std::string http_response(std::string_view request, const Metrics& metrics) {
    // Only the request line matters: "METHOD /path HTTP/1.1". Headers are ignored.
    const std::string_view line = request.substr(0, request.find("\r\n"));
    const std::size_t first_space = line.find(' ');
    const std::size_t second_space = line.find(' ', first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos) {
        return response("400 Bad Request", "text/plain", "bad request\n");
    }
    const std::string_view method = line.substr(0, first_space);
    const std::string_view path = line.substr(first_space + 1, second_space - first_space - 1);

    if (method != "GET") {
        return response("405 Method Not Allowed", "text/plain", "only GET\n");
    }
    if (path == "/metrics") {
        return response("200 OK", "text/plain; version=0.0.4", render_prometheus(metrics));
    }
    if (path == "/health") {
        return response("200 OK", "text/plain", "ok\n");
    }
    if (path == "/ready") {
        return metrics.draining.load(std::memory_order_relaxed)
                   ? response("503 Service Unavailable", "text/plain", "draining\n")
                   : response("200 OK", "text/plain", "ready\n");
    }
    return response("404 Not Found", "text/plain", "not found\n");
}

}  // namespace net

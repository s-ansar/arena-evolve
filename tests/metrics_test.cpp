#include <gtest/gtest.h>

#include <string>

#include "net/metrics.hpp"

TEST(Metrics, RendersPrometheusTextFormat) {
    net::Metrics m;
    m.matches_won = 3;
    m.matches_drawn = 1;
    m.ticks = 500;
    m.last_jitter_seconds = 0.0001;
    const std::string text = net::render_prometheus(m);
    EXPECT_NE(text.find("# TYPE arena_matches_total counter\n"), std::string::npos);
    EXPECT_NE(text.find("arena_matches_total{result=\"win\"} 3\n"), std::string::npos);
    EXPECT_NE(text.find("arena_matches_total{result=\"draw\"} 1\n"), std::string::npos);
    EXPECT_NE(text.find("arena_ticks_total 500\n"), std::string::npos);
    EXPECT_NE(text.find("arena_last_match_tick_jitter_seconds 0.0001\n"), std::string::npos);
    EXPECT_NE(text.find("arena_match_running 0\n"), std::string::npos);
}

TEST(Metrics, HttpRoutes) {
    const net::Metrics m;
    const std::string metrics = net::http_response("GET /metrics HTTP/1.1\r\nHost: x\r\n\r\n", m);
    EXPECT_EQ(metrics.rfind("HTTP/1.1 200 OK\r\n", 0), 0u);
    EXPECT_NE(metrics.find("arena_ticks_total 0"), std::string::npos);

    const std::string health = net::http_response("GET /health HTTP/1.1\r\n\r\n", m);
    EXPECT_EQ(health.rfind("HTTP/1.1 200 OK\r\n", 0), 0u);
    EXPECT_NE(health.find("\r\n\r\nok\n"), std::string::npos);

    EXPECT_EQ(net::http_response("GET /ready HTTP/1.1\r\n\r\n", m).rfind("HTTP/1.1 200 OK", 0), 0u);
    EXPECT_EQ(net::http_response("GET /nope HTTP/1.1\r\n\r\n", m).rfind("HTTP/1.1 404", 0), 0u);
    EXPECT_EQ(net::http_response("POST /metrics HTTP/1.1\r\n\r\n", m).rfind("HTTP/1.1 405", 0), 0u);
    EXPECT_EQ(net::http_response("garbage", m).rfind("HTTP/1.1 400", 0), 0u);
    EXPECT_EQ(net::http_response("", m).rfind("HTTP/1.1 400", 0), 0u);
}

TEST(Metrics, ReadyTurnsTo503WhenDraining) {
    net::Metrics m;
    m.draining = true;
    const std::string ready = net::http_response("GET /ready HTTP/1.1\r\n\r\n", m);
    EXPECT_EQ(ready.rfind("HTTP/1.1 503 Service Unavailable\r\n", 0), 0u);
    EXPECT_NE(net::render_prometheus(m).find("arena_draining 1\n"), std::string::npos);
    // Liveness stays OK: a draining server is healthy, it is just not taking new games.
    EXPECT_EQ(net::http_response("GET /health HTTP/1.1\r\n\r\n", m).rfind("HTTP/1.1 200 OK", 0), 0u);
}

TEST(Metrics, ContentLengthMatchesTheBody) {
    const net::Metrics m;
    const std::string reply = net::http_response("GET /health HTTP/1.1\r\n\r\n", m);
    const std::size_t body_start = reply.find("\r\n\r\n") + 4;
    EXPECT_NE(reply.find("Content-Length: " + std::to_string(reply.size() - body_start) + "\r\n"),
              std::string::npos);
}

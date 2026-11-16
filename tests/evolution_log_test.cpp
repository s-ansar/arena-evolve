#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>

#include "ai/evolution_log.hpp"

TEST(EvolutionLog, WritesTheDocumentedCsv) {
    std::ostringstream out;
    ai::write_evolution_log(out, {{0, 16.0, 2.5}, {1, 33.25, 4.0}});
    EXPECT_EQ(out.str(), "generation,best,mean\n0,16,2.5\n1,33.25,4\n");
}

TEST(EvolutionLog, RoundTripsExactly) {
    const std::vector<ai::GenerationStats> rows{{0, 16.0, 2.3203125}, {1, 45.193847, 31.28}};
    std::stringstream buffer;
    ai::write_evolution_log(buffer, rows);
    EXPECT_EQ(ai::read_evolution_log(buffer), rows);
}

TEST(EvolutionLog, RejectsMissingHeader) {
    std::istringstream in("0,16,2.5\n");
    EXPECT_THROW(ai::read_evolution_log(in), std::runtime_error);
}

TEST(EvolutionLog, RejectsMalformedLines) {
    for (const char* bad : {"generation,best,mean\n0;16;2.5\n", "generation,best,mean\n0,16\n",
                            "generation,best,mean\n0,16,2.5,extra\n"}) {
        std::istringstream in(bad);
        EXPECT_THROW(ai::read_evolution_log(in), std::runtime_error) << bad;
    }
}

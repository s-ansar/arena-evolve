#include "ai/evolution_log.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ai {

namespace {
constexpr const char* kHeader = "generation,best,mean";
}

void write_evolution_log(std::ostream& out, const std::vector<GenerationStats>& rows) {
    out << kHeader << '\n' << std::setprecision(17);
    for (const GenerationStats& row : rows) {
        out << row.generation << ',' << row.best << ',' << row.mean << '\n';
    }
}

std::vector<GenerationStats> read_evolution_log(std::istream& in) {
    std::string line;
    if (!std::getline(in, line) || line != kHeader) {
        throw std::runtime_error("evolution log must start with: " + std::string(kHeader));
    }
    std::vector<GenerationStats> rows;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        // Parse "generation,best,mean": read a number, expect a comma, and so on, and make
        // sure nothing is left over at the end.
        std::istringstream fields(line);
        GenerationStats row;
        char comma1 = 0;
        char comma2 = 0;
        if (!(fields >> row.generation >> comma1 >> row.best >> comma2 >> row.mean) || comma1 != ',' ||
            comma2 != ',' || !(fields >> std::ws).eof()) {
            throw std::runtime_error("bad evolution log line: " + line);
        }
        rows.push_back(row);
    }
    return rows;
}

}  // namespace ai

// Microbenchmarks with Google Benchmark. Run a Release build for meaningful numbers:
//   cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release
//   ./build-release/sim_bench
// The library repeats each loop until the timing is stable and reports the time per call.

#include <benchmark/benchmark.h>

#include <cmath>

#include "net/protocol.hpp"
#include "sim/arena.hpp"

namespace {

// n small robots on a grid inside the ring, standing still and not touching, so every
// step does the same work: move everyone, test every pair, check the ring edge.
sim::Arena grid_of(int n) {
    sim::Arena arena;
    const int side = static_cast<int>(std::ceil(std::sqrt(n)));
    for (int i = 0; i < n; ++i) {
        sim::Robot robot;
        robot.radius = 0.01;
        robot.position = {-0.6 + 1.2 * (i % side) / side, -0.6 + 1.2 * (i / side) / side};
        arena.robots.push_back(robot);
    }
    return arena;
}

void BM_Step(benchmark::State& state) {
    sim::Arena arena = grid_of(static_cast<int>(state.range(0)));
    for (auto _ : state) {
        sim::step(arena, sim::Seconds{1.0 / 60.0});
        benchmark::DoNotOptimize(arena);  // stop the compiler deleting the "unused" work
    }
    state.SetComplexityN(state.range(0));
}
// 2 robots is a real match; up to 64 shows how the all-pairs collision check grows.
BENCHMARK(BM_Step)->RangeMultiplier(2)->Range(2, 64)->Complexity(benchmark::oNSquared);

net::State two_robot_state() {
    net::State state;
    state.tick = 1000;
    state.robots = {{-0.5f, 0.0f, 1.0f, 0.0f, true}, {0.5f, 0.0f, -1.0f, 0.0f, true}};
    return state;
}

void BM_EncodeState(benchmark::State& state) {
    const net::State message = two_robot_state();
    for (auto _ : state) {
        benchmark::DoNotOptimize(net::encode(message));
    }
}
BENCHMARK(BM_EncodeState);

void BM_DecodeState(benchmark::State& state) {
    const std::vector<std::uint8_t> frame = net::encode(two_robot_state());
    for (auto _ : state) {
        std::vector<std::uint8_t> buffer = frame;  // try_decode consumes its input
        benchmark::DoNotOptimize(net::try_decode(buffer));
    }
}
BENCHMARK(BM_DecodeState);

}  // namespace

BENCHMARK_MAIN();

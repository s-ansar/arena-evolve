# syntax=docker/dockerfile:1
# Multi-stage build: compile once in a full toolchain image, then copy only the finished
# programs into small runtime images. Three images come out of this one file:
#   docker build --target server  -t arena-evolve/server  .   game server (+ /metrics, /health)
#   docker build --target bot     -t arena-evolve/bot     .   controller client (chase or evolved)
#   docker build --target fitness -t arena-evolve/fitness .   fitness worker for the platform phase

# ---- Stage 1: build ----
FROM ubuntu:24.04 AS build
# --no-install-recommends and removing the apt lists keep this layer smaller.
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake ninja-build \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
# Copy only what the build needs, so editing docs does not invalidate the build cache.
COPY CMakeLists.txt ./
COPY src ./src
# No tests or benchmarks: they would download GoogleTest and Google Benchmark, and the
# images only need the programs. Qt is not installed, so the GUI is skipped automatically.
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DARENA_BUILD_TESTS=OFF -DARENA_BUILD_BENCHMARKS=OFF \
 && cmake --build build --target arena_server arena_bot arena_fitness arena_evolve

# ---- Stage 2: shared runtime base ----
# The same Ubuntu release as the build stage, so the C++ runtime library (libstdc++, already
# in the base image) matches the compiler. Programs run as an unprivileged user, not root.
FROM ubuntu:24.04 AS runtime
# A version label, so the same code can be built as distinct image versions (used to test
# Kubernetes rolling updates):  docker build --build-arg ARENA_VERSION=2 ...
ARG ARENA_VERSION=dev
LABEL org.opencontainers.image.version=$ARENA_VERSION
# A fixed numeric UID: Kubernetes can only enforce runAsNonRoot when the image user is a
# number, not a name.
RUN useradd --system --uid 10001 --no-create-home arena
WORKDIR /app
USER 10001

# ---- Final images ----
FROM runtime AS server
COPY --from=build /src/build/arena_server /app/
# 5555/tcp controllers, 5555/udp watchers, 8080/tcp metrics and health.
EXPOSE 5555/tcp 5555/udp 8080/tcp
ENTRYPOINT ["/app/arena_server"]
CMD ["5555", "8080"]

FROM runtime AS bot
COPY --from=build /src/build/arena_bot /app/
COPY docs/genomes /app/genomes
ENTRYPOINT ["/app/arena_bot"]
CMD ["server", "5555"]

FROM runtime AS fitness
COPY --from=build /src/build/arena_fitness /src/build/arena_evolve /app/
COPY docs/genomes /app/genomes
ENTRYPOINT ["/app/arena_fitness"]
CMD ["/app/genomes/seed1.txt", "test"]

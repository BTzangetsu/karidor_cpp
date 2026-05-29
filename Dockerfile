# ── Stage 1 : build ───────────────────────────────────────────────────────────
# Full Debian image with GCC, CMake, and static libs.
# This stage is heavy (~500MB) but is DISCARDED after compilation.
FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libssl-dev \
    libc6-dev \
    libstdc++-12-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the full source tree into the builder
COPY . .

# Configure and build in Release mode
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release -j$(nproc)


# ── Stage 2 : final image ─────────────────────────────────────────────────────
# Minimal scratch image — contains ONLY the binary + static assets.
# No compiler, no headers, no shared libs. Typical size: ~10-20MB.
FROM scratch

WORKDIR /app

# Copy the self-contained binary from the builder stage
COPY --from=builder /app/build/karidor .

# Copy static assets that Crow serves at runtime
COPY --from=builder /app/build/static    ./static
COPY --from=builder /app/build/templates ./templates

EXPOSE 5000

CMD ["./karidor"]
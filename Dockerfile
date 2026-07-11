FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app


COPY include/ ./include/
COPY src/ ./src/
COPY CMakeLists.txt .

RUN mkdir build && \
    cd build && \
    cmake .. && \
    make

FROM debian:bookworm-slim

WORKDIR /app

COPY --from=builder /app/build/BoltKV_db .

EXPOSE 6380

VOLUME ["/app/data"]

CMD ["./BoltKV_db"]
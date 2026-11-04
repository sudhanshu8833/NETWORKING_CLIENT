# NETWORKING_CLIENT

A single-threaded HTTP/1.1 server in C++ built directly on the Linux sockets
API and an `epoll` event loop, with a WebSocket upgrade path.

## What it does

- Opens a listening socket and drives all I/O from one `epoll` instance
  (`epoll_create1`, `EPOLLIN`, `epoll_ctl`) - no per-connection thread.
- Parses incoming HTTP requests and detects a valid WebSocket handshake
  (`Upgrade: websocket`, `Connection: upgrade`, `Sec-WebSocket-Key`,
  `Sec-WebSocket-Version: 13`).
- Completes the upgrade using OpenSSL for the `Sec-WebSocket-Accept` digest,
  then encodes and decodes WebSocket data frames (text frames, with the
  extended 16- and 64-bit length forms).

## Build and run

Requires C++17, CMake 3.10+, and OpenSSL.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/http_server          # listens on 0.0.0.0:8080

## Layout

- `server_linux.cpp`          - entry point; constructs and starts the server
- `http_tcpServer_linux.*`    - socket setup, the epoll loop, WebSocket frames
- `http_tcpServer_parser.*`   - HTTP request and upgrade-request parsing

# Roadmap

## Current Status: v0.1.0 (Alpha)

Core networking primitives are implemented. The library provides IPv4/IPv6 addresses, socket addresses, TCP endpoints, a basic socket wrapper, and error handling.

---

## v0.2.0 — UDP & Socket Improvements

**Goal:** Complete the transport layer and make sockets production-ready.

- [x] `Socket<Ip<6>>` — IPv6 socket specialization
- [x] `UdpSocket<T>` — UDP datagram socket wrapper
- [x] `Udp<T>` — UDP endpoint struct (analogous to `Tcp<T>`)
- [x] Socket options: `set_reuse_addr()`, `set_nonblocking()`, `set_timeout()`
- [x] `Socket::shutdown()` — Graceful half-close (SHUT_RD, SHUT_WR, SHUT_RDWR)
- [x] Improve error mapping — Map platform error codes (`WSAGetLastError` / `errno`) to `core::Error`

---

## v0.3.0 — Async & Event Loop

**Goal:** Non-blocking I/O and event-driven networking.

- [x] Non-blocking socket mode
- [x] `Poll` / `Select` wrapper for I/O multiplexing
- [x] `EventLoop` — Single-threaded event loop
- [x] Async `connect()`, `accept()`, `send()`, `recv()`
- [x] Callback-based or coroutine-based API (evaluate C++23 coroutine support)

---

## v0.4.0 — Higher-Level Protocols

**Goal:** Provide ready-to-use protocol implementations.

- [x] `HttpRequest` / `HttpResponse` — Basic HTTP/1.1 parser
- [x] `HttpClient` — Simple HTTP client
- [x] `HttpServer` — Lightweight HTTP server
- [x] `WebSocket` — WebSocket protocol support
- [x] URL parsing utility

---

## v0.5.0 — Security & TLS

**Goal:** Encrypted communication support.

- [x] TLS/SSL integration (OpenSSL or platform-native)
- [x] `TlsSocket<T>` — Encrypted socket wrapper
- [x] Certificate management utilities
- [x] HTTPS support in `HttpClient` / `HttpServer`

---

## v0.6.0 — DNS & Network Utilities

**Goal:** DNS resolution and network discovery.

- [x] `Dns::resolve(hostname)` — Hostname to IP resolution
- [x] `Dns::reverse(ip)` — Reverse DNS lookup
- [x] `NetworkInterface` — List local network interfaces
- [x] Subnet / CIDR utilities (`Subnet<T>`, `contains()`, `mask()`)
- [x] Ping / ICMP utility

---

## v1.0.0 — Stable Release

**Goal:** Production-ready, well-tested, fully documented.

- [x] Comprehensive unit test suite (Google Test or Catch2)
- [x] Benchmarks for critical paths
- [x] CI/CD pipeline (GitHub Actions)
- [x] Package manager support (vcpkg, Conan)
- [x] Complete Doxygen API documentation
- [x] Example programs for each use case
- [x] Performance optimization pass
- [x] API stability guarantee

---

## Future Ideas (Post v1.0)

- **Multicast** support
- **Unix domain sockets**
- **Serial port** communication
- **gRPC / Protocol Buffers** integration
- **io_uring** (Linux) / **IOCP** (Windows) backends
- **Connection pooling**
- **Rate limiting** and traffic shaping
- **Proxy** support (SOCKS5, HTTP CONNECT)

---

## Contributing

Want to help? See [CONTRIBUTING.md](../CONTRIBUTING.md) for how to get started. Pick any unchecked item from the roadmap and open a PR!

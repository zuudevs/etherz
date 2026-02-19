# Roadmap

## Current Status: v0.1.0 (Alpha)

Core networking primitives are implemented. The library provides IPv4/IPv6 addresses, socket addresses, TCP endpoints, a basic socket wrapper, and error handling.

---

## v0.2.0 — UDP & Socket Improvements

**Goal:** Complete the transport layer and make sockets production-ready.

- [ ] `Socket<Ip<6>>` — IPv6 socket specialization
- [ ] `UdpSocket<T>` — UDP datagram socket wrapper
- [ ] `Udp<T>` — UDP endpoint struct (analogous to `Tcp<T>`)
- [ ] Socket options: `set_reuse_addr()`, `set_nonblocking()`, `set_timeout()`
- [ ] `Socket::shutdown()` — Graceful half-close (SHUT_RD, SHUT_WR, SHUT_RDWR)
- [ ] Improve error mapping — Map platform error codes (`WSAGetLastError` / `errno`) to `core::Error`

---

## v0.3.0 — Async & Event Loop

**Goal:** Non-blocking I/O and event-driven networking.

- [ ] Non-blocking socket mode
- [ ] `Poll` / `Select` wrapper for I/O multiplexing
- [ ] `EventLoop` — Single-threaded event loop
- [ ] Async `connect()`, `accept()`, `send()`, `recv()`
- [ ] Callback-based or coroutine-based API (evaluate C++23 coroutine support)

---

## v0.4.0 — Higher-Level Protocols

**Goal:** Provide ready-to-use protocol implementations.

- [ ] `HttpRequest` / `HttpResponse` — Basic HTTP/1.1 parser
- [ ] `HttpClient` — Simple HTTP client
- [ ] `HttpServer` — Lightweight HTTP server
- [ ] `WebSocket` — WebSocket protocol support
- [ ] URL parsing utility

---

## v0.5.0 — Security & TLS

**Goal:** Encrypted communication support.

- [ ] TLS/SSL integration (OpenSSL or platform-native)
- [ ] `TlsSocket<T>` — Encrypted socket wrapper
- [ ] Certificate management utilities
- [ ] HTTPS support in `HttpClient` / `HttpServer`

---

## v0.6.0 — DNS & Network Utilities

**Goal:** DNS resolution and network discovery.

- [ ] `Dns::resolve(hostname)` — Hostname to IP resolution
- [ ] `Dns::reverse(ip)` — Reverse DNS lookup
- [ ] `NetworkInterface` — List local network interfaces
- [ ] Subnet / CIDR utilities (`Subnet<T>`, `contains()`, `mask()`)
- [ ] Ping / ICMP utility

---

## v1.0.0 — Stable Release

**Goal:** Production-ready, well-tested, fully documented.

- [ ] Comprehensive unit test suite (Google Test or Catch2)
- [ ] Benchmarks for critical paths
- [ ] CI/CD pipeline (GitHub Actions)
- [ ] Package manager support (vcpkg, Conan)
- [ ] Complete Doxygen API documentation
- [ ] Example programs for each use case
- [ ] Performance optimization pass
- [ ] API stability guarantee

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

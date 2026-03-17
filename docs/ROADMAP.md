# Roadmap

## Current Status: v4.0.0 (Stable)

Core networking library is stable and production-ready. All milestones (v0.1–v4.0) are complete, including connection pooling, multicast, proxy/SOCKS5, rate limiting, platform I/O backends, Unix/serial, HTTP/2, gRPC, coroutines, DTLS, MQTT, FTP/SMTP, mDNS/DNS-SD, compression, NAT traversal, WebSocket client/server, REST utilities, and thread pool.

---

## v4.0.0 — Thread Pool & Parallel I/O

**Goal:** General-purpose thread pool and multi-threaded TCP server.

- [x] `async/thread_pool.hpp` — Worker pool with `std::future`-based task submission, graceful shutdown
- [x] `async/parallel_socket.hpp` — `ParallelAcceptor<T>` multi-threaded TCP acceptor with configurable workers
- [x] `examples/threaded_server.cpp` — Multi-threaded HTTP server demo
- [x] `tests/test_thread_pool.cpp` — Thread pool unit tests

---

## v3.3.0 — JSON & REST Utilities

**Goal:** REST client with JSON support via zuu-json.

- [x] `protocol/rest_client.hpp` — `RestClient` with typed REST verbs (GET/POST/PUT/PATCH/DELETE), Bearer/Basic auth, JSON body serialization via zuu-json
- [x] Integrated `third_party/zuu-json` for parsing and serialization
- [x] `examples/rest_api.cpp` — REST API usage demo
- [x] `tests/test_rest.cpp` — REST client, auth, JSON parsing tests

---

## v3.2.0 — WebSocket Client & Server

**Goal:** High-level WebSocket client and server with auto ping/pong.

- [x] `protocol/ws_client.hpp` — `WsClient` with HTTP upgrade, auto ping/pong, fragmented message reassembly, close handshake, masking
- [x] `protocol/ws_server.hpp` — `WsServer` with per-connection callbacks, broadcast, auto pong
- [x] `examples/ws_chat.cpp` — WebSocket chat demo (server + client threads)
- [x] `tests/test_ws_client.cpp` — Handshake, frame encode/decode, close codes tests

---

## v3.1.0 — NAT Traversal & UPnP

**Goal:** NAT traversal utilities for P2P and port mapping.

- [x] `net/upnp.hpp` — UPnP IGD client: SSDP discovery, SOAP port mapping (add/delete), external IP query
- [x] `net/stun.hpp` — STUN client (RFC 5389): binding request, XOR-MAPPED-ADDRESS, NAT type detection
- [x] `core/error.hpp` — Added `UpnpError`, `StunError` error codes
- [x] `examples/nat_traversal.cpp` — Combined STUN + UPnP demo
- [x] `tests/test_upnp.cpp`, `tests/test_stun.cpp` — Offline packet parsing tests

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

## v1.1.0 — Connection Pooling & Multicast

**Goal:** Reusable connections and multicast networking.

- [x] `ConnectionPool<T>` — Reusable TCP connection pool (max connections, idle timeout, keep-alive)
- [x] `HttpClient` keep-alive — Persistent connections via `Connection: keep-alive`
- [x] `MulticastSocket<T>` — `join_group()`, `leave_group()`, `set_ttl()`, send/recv multicast datagrams
- [x] Multicast example program

---

## v1.2.0 — Proxy & SOCKS5 Support

**Goal:** Route traffic through proxy servers.

- [x] `ProxyConfig` — Proxy type (SOCKS5, HTTP), host, port, credentials
- [x] `Socks5Client` — SOCKS5 handshake (RFC 1928), username/password auth, TCP CONNECT
- [x] HTTP CONNECT tunneling in `HttpClient`
- [x] `HttpClient::set_proxy()` / `TlsSocket` proxy tunneling
- [x] Proxy example program

---

## v1.3.0 — Rate Limiting & Traffic Shaping

**Goal:** Control bandwidth and request rates.

- [x] `RateLimiter` — Token-bucket algorithm, configurable rate + burst size
- [x] `ThrottledSocket<T>` — Socket wrapper enforcing send/recv rate limits
- [x] `HttpServer` middleware — Per-route and global rate limiting
- [x] Bandwidth monitoring — Track bytes sent/received per socket

---

## v1.4.0 — Platform I/O Backends

**Goal:** High-performance, platform-native async I/O.

- [x] `IoBackend` abstraction — Platform-agnostic async I/O interface (concept-based)
- [x] IOCP backend (Windows) — `CreateIoCompletionPort`, overlapped I/O
- [x] io_uring backend (Linux) — `io_uring_setup`, submission/completion ring
- [x] kqueue backend (macOS/BSD) — `kqueue()`, `kevent()` integration
- [x] `EventLoop` refactor — Pluggable backend selection at compile time
- [x] Benchmarks — poll vs. native backend comparison

---

## v1.5.0 — Unix Domain Sockets & Serial Port

**Goal:** IPC and hardware communication.

- [x] `UnixSocket` — AF_UNIX stream + datagram support (POSIX only)
- [x] `UnixSocketAddress` — Path-based addressing
- [x] `SerialPort` — Open, configure (baud rate, parity, stop bits), read/write
- [x] Platform abstraction for serial (`CreateFile` on Windows, `termios` on POSIX)
- [x] Example programs for Unix sockets and serial port

---

## v2.0.0 — gRPC & Protocol Buffers

**Goal:** High-level RPC framework built on HTTP/2.

- [x] HTTP/2 framing — HPACK header compression, stream multiplexing
- [x] `GrpcChannel` — Channel to a gRPC server
- [x] `GrpcServer` — Service registration and dispatch
- [x] Protocol Buffers codec — Serialize/deserialize `.proto` messages
- [x] Streaming support — Unary, server-streaming, client-streaming, bidirectional
- [x] gRPC example program

---

## v2.1.0 — C++20 Coroutines & Async Improvements

**Goal:** First-class coroutine support for async I/O.

- [x] `Task<T>` — Lazy coroutine return type with `co_await` support for socket ops
- [x] `Generator<T>` — Coroutine-based lazy sequence generator for streaming data
- [x] `Timer` — Async timer with one-shot and repeating modes
- [x] Coroutine echo server example

---

## v2.2.0 — DTLS & Secure UDP

**Goal:** Encrypted datagram transport.

- [x] `DtlsContext` — DTLS session configuration, cipher suites
- [x] `DtlsSocket<T>` — Encrypted UDP wrapper with DTLS 1.2/1.3 handshake
- [x] DTLS chat example

---

## v2.3.0 — MQTT Protocol

**Goal:** IoT messaging protocol support.

- [x] MQTT 3.1.1/5.0 packet types, serialization, QoS levels
- [x] `MqttClient` — Connect, publish, subscribe, keep-alive
- [x] MQTT publish/subscribe example

---

## v2.4.0 — FTP & SMTP Protocols

**Goal:** File transfer and email protocols.

- [x] `FtpClient` — Login, list, upload, download, passive mode
- [x] `SmtpClient` — Send emails with STARTTLS, attachments, MIME encoding
- [x] FTP download and email sending examples

---

## v2.5.0 — mDNS & Service Discovery

**Goal:** Zero-config networking and LAN discovery.

- [x] `MdnsResponder` / `MdnsQuery` — Multicast DNS (RFC 6762)
- [x] `ServiceBrowser` / `ServiceRegistrar` — DNS-SD (RFC 6763)
- [x] LAN service discovery example

---

## v3.0.0 — Compression & Advanced Streams

**Goal:** Data compression and advanced stream processing.

- [x] `Deflate` / `Gzip` / `Zstd` codec — Compress/decompress buffers
- [x] `CompressedSocket<T>` — Transparent per-connection compression
- [x] `ByteStream` — Ring-buffered read/write with backpressure
- [x] `Pipe` — In-process bidirectional byte stream
- [x] File transfer with compression example

---

## Contributing

Want to help? See [CONTRIBUTING.md](../CONTRIBUTING.md) for how to get started.

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.5.0] — 2026-02-19

### Added

- **`etherz::security` namespace** — TLS/SSL security layer

- **`TlsMethod`** / **`TlsVerifyMode`** / **`TlsRole`** enums
- **`TlsContext`** — TLS configuration (method, verify mode, hostname, cert/key paths)
  - `TlsContext::client(hostname)` / `TlsContext::server()` factory methods

- **`TlsSocket<T>`** — Encrypted socket wrapper using Windows SChannel (SSPI)
  - `create(TlsContext&)` — socket + credential acquisition
  - `connect(addr)` — TCP connect + full TLS handshake loop
  - `send()` / `recv()` — EncryptMessage / DecryptMessage
  - RAII lifecycle with move semantics

- **`CertInfo`** — Certificate information struct
  - `make_self_signed_info()` factory for testing

- **HTTPS support** in `HttpClient` — auto-detects `https://` scheme

### Changed

- `HttpClient::send_request()` routes to `send_plain()` or `send_secure()`
- `error.hpp` — added `HandshakeFailed`, `CertificateError` error codes

---

## [0.4.0] — 2026-02-19

### Added

- **`etherz::protocol` namespace** — Higher-level protocol implementations

- **`Url`** — URL parser
  - `Url::parse()` extracts scheme, host, port, path, query, fragment
  - `to_string()` reconstruction, default port inference (http→80, https→443)

- **`HttpMethod`** / **`HttpStatus`** enums with string converters
- **`HttpHeaders`** — Case-insensitive key-value header map
- **`HttpRequest`** / **`HttpResponse`** — Serialization and display
- **`http_parser`** — `parse_request()` / `parse_response()` from raw HTTP strings

- **`HttpClient`** — Synchronous HTTP/1.1 client
  - `get(Url)` / `post(Url, body)` → `HttpClient::Result`

- **`HttpServer`** — Lightweight HTTP server with route registration
  - `route(method, path, handler)` / `get()` / `post()` shorthands
  - `listen(addr)` / `handle_one()` / `stop()`

- **`WsOpcode`** / **`WsFrame`** — WebSocket frame types
  - `set_text()` / `set_binary()` / `payload_text()`
  - `ws_encode_frame()` / `ws_decode_frame()` — RFC 6455 framing
  - `ws_handshake_request()` / `ws_handshake_response()`

---

## [0.3.0] — 2026-02-19

### Added

- **`etherz::async` namespace** — New async I/O subsystem

- **`PollEvent`** — Bitmask enum: `ReadReady`, `WriteReady`, `Error`, `HangUp`
  - Bitwise operators (`|`, `&`, `|=`) and `has_event()` helper

- **`PollEntry`** — Struct for poll operations: `fd`, `requested`, `returned`

- **`poll(span<PollEntry>, timeout_ms)`** — Platform-abstracted I/O multiplexing
  - Uses `WSAPoll` (Windows) / `::poll` (POSIX)
  - Stack allocation optimization for ≤64 entries

- **`EventLoop`** — Single-threaded event loop
  - `add(fd, interest, callback)` / `remove(fd)` for socket registration
  - `run_once(timeout_ms)` — single poll + dispatch cycle
  - `run()` / `stop()` — continuous loop with graceful shutdown
  - Callback type: `std::function<void(socket_t, PollEvent)>`

- **`AsyncSocket<T>`** — Callback-based async TCP socket
  - Auto non-blocking mode on `create()`
  - `async_connect(addr, loop, callback)` — wait for WriteReady
  - `async_accept(loop, callback)` — wait for ReadReady, continuous listening
  - `async_send(data, loop, callback)` / `async_recv(buffer, loop, callback)`
  - Delegates socket options and lifecycle to underlying `Socket<T>`

---

## [0.2.0] — 2026-02-19

### Added

- **`Socket<Ip<6>>`** — IPv6 TCP socket specialization
  - Full parity with `Socket<Ip<4>>`: `create`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`
  - Uses `AF_INET6` / `sockaddr_in6` with helper converters `fill_in6_addr` / `extract_ip6`

- **`Udp<Ip<4>>` and `Udp<Ip<6>>`** — UDP endpoint structs
  - Analogous to `Tcp<T>`: `address` + `port` + `display()`
  - Three-way comparison (`<=>`)

- **`UdpSocket<Ip<4>>`** — RAII UDP datagram socket wrapper
  - `create()`, `bind()`, `send_to()`, `recv_from()`, `close()`
  - `RecvResult` struct with bytes, sender address, and error
  - Full socket options and shutdown support

- **Socket Options** — Added to both `Socket` and `UdpSocket`:
  - `set_reuse_addr(bool)` → `SO_REUSEADDR`
  - `set_nonblocking(bool)` → `FIONBIO` (Win) / `O_NONBLOCK` (POSIX)
  - `set_timeout(uint32_t ms)` → `SO_RCVTIMEO` + `SO_SNDTIMEO`

- **`Socket::shutdown(ShutdownMode)`** — Graceful half-close
  - `ShutdownMode::Read`, `Write`, `Both`
  - Maps to `SD_RECEIVE/SD_SEND/SD_BOTH` (Win) / `SHUT_RD/WR/RDWR` (POSIX)

- **Platform Error Mapping**
  - `from_platform_error(int)` → maps WSA/errno codes to `core::Error`
  - `last_platform_error()` → get + map current error
  - New error codes: `ShutdownFailed`, `OptionFailed`, `WouldBlock`

### Changed

- `AcceptResult` now stores raw `impl::socket_t` with `take_client()` factory method (fixes incomplete type issue)
- All socket operations now use `last_platform_error()` for accurate error mapping
- Shared helpers: `impl::ensure_wsa()`, `impl::set_sock_opt()`, `impl::set_nonblocking_impl()`

---

## [0.1.0] — 2026-02-19

### Added

- **`Ip<4>` (IPv4)** — Full implementation
  - Construction from 4 octets, `uint32_t`, C array, and `string_view`
  - CTAD deduction guide: `Ip(a, b, c, d)` → `Ip<4>`
  - Arithmetic operators: `+`, `-`, `+=`, `-=`, `++`, `--`
  - Three-way comparison (`<=>`)
  - Converters: `to_uint32()`, `from_uint32()`, `to_network()`
  - `fill()` and `display()` methods

- **`Ip<6>` (IPv6)** — Full implementation
  - Construction from 8 `uint16_t` groups
  - String parsing with `::` abbreviation support
  - Increment/decrement operators with ripple-carry
  - Three-way comparison, `fill()`, `display()`

- **`SocketAddress<Ip<4>>` and `SocketAddress<Ip<6>>`**
  - IP + port pair with accessors
  - IPv4 string constructor: `SocketAddress("ip", "port")`
  - `display()` methods with proper bracket notation for IPv6

- **`Tcp<Ip<4>>` and `Tcp<Ip<6>>`**
  - Lightweight TCP endpoint structs
  - `address` and `port` public members
  - Three-way comparison and `display()`

- **`Socket<Ip<4>>`** — RAII TCP socket wrapper
  - Platform-aware: WinSock2 (Windows) / POSIX (Linux/macOS)
  - Operations: `create`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `close`
  - Move semantics (non-copyable)
  - `AcceptResult` structured binding support

- **`core::Error`** — Error handling
  - 20 error codes covering common networking errors
  - `error_message()`, `is_ok()`, `is_error()` helpers

- **`impl::IpBase<D, T, N>`** — CRTP base class for IP addresses
- **`impl::WsaGuard`** — WinSock RAII initializer (Windows only)
- **CMake build system** with C++23, strict warnings, and cross-platform support
- **Demo application** (`src/main.cpp`) showcasing all features
- **Documentation**: README, BUILD, ARCHITECTURE, API, QUICKSTART, ROADMAP

---

## [Unreleased]

See [ROADMAP.md](ROADMAP.md) for planned features.

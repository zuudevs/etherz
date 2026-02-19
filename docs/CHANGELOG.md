# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

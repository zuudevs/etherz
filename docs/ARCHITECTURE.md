# Architecture

## Overview

Etherz is a **header-only** C++23 networking library organized into two modules:

```
etherz/
├── core/    → Error handling, shared utilities
└── net/     → Networking primitives (IP, Socket, TCP)
```

## Namespace Structure

```
etherz::                     # Root namespace (version, metadata)
├── core::                   # Core utilities
│   └── Error                # Error enum + helpers
└── net::                    # Networking module
    ├── impl::               # Internal implementation details
    │   ├── IpBase<D, T, N>  # CRTP base for IP addresses
    │   ├── socket_t         # Platform socket type alias
    │   └── WsaGuard         # WinSock RAII initializer (Windows)
    ├── Ip<4>                # IPv4 address
    ├── Ip<6>                # IPv6 address
    ├── SocketAddress<T>     # IP + Port pair
    ├── Tcp<T>               # TCP endpoint struct
    └── Socket<T>            # RAII TCP socket wrapper
```

## Design Patterns

### CRTP (Curiously Recurring Template Pattern)

IP address classes use CRTP via `impl::IpBase<Derived, T, N>` to share common functionality while allowing specialization-specific behavior:

```
IpBase<Derived, T, N>
├── Ip<4> : IpBase<Ip<4>, uint8_t,  4>   → 4 octets
└── Ip<6> : IpBase<Ip<6>, uint16_t, 8>   → 8 groups
```

**Why CRTP?** It provides zero-cost static polymorphism. The base class holds the address array and default constructors, while derived classes add version-specific parsing and arithmetic.

### Template Specialization

The library uses **explicit full specialization** for all type-parameterized classes:

```cpp
template <uint8_t Ipv> class Ip;     // Primary: static_assert guard
template <> class Ip<4> { ... };     // Specialization: IPv4
template <> class Ip<6> { ... };     // Specialization: IPv6
```

This approach ensures:
- Compile-time validation (invalid versions fail at `static_assert`)
- Each specialization is fully independent
- No virtual dispatch overhead

### RAII (Resource Acquisition Is Initialization)

`Socket<T>` manages the OS socket handle with automatic cleanup:

```
Socket()     → fd_ = INVALID_SOCKET
create()     → fd_ = ::socket(...)
~Socket()    → close(fd_)
```

Move semantics are supported; copy is deleted. On Windows, `impl::WsaGuard` ensures `WSAStartup`/`WSACleanup` follows RAII.

## Platform Abstraction

Socket operations are abstracted behind platform-specific typedefs and functions:

| Abstraction | Windows | POSIX |
|-------------|---------|-------|
| `socket_t` | `SOCKET` | `int` |
| `invalid_socket` | `INVALID_SOCKET` | `-1` |
| `close_socket()` | `closesocket()` | `close()` |
| `last_error()` | `WSAGetLastError()` | `errno` |

## Dependency Graph

```
etherz.hpp (standalone - version metadata)

core/error.hpp (standalone - no dependencies)

net/internet_protocol.hpp
 └── <array>, <cstdint>, <print>, <concepts>, <compare>, <bit>

net/socket_address.hpp
 └── internet_protocol.hpp

net/tcp.hpp
 └── internet_protocol.hpp

net/socket.hpp
 ├── internet_protocol.hpp
 ├── socket_address.hpp
 ├── core/error.hpp
 └── <winsock2.h> | <sys/socket.h>  (platform-specific)
```

## Key C++23 Features Used

| Feature | Usage |
|---------|-------|
| `std::print` | Formatted output in `display()` methods |
| `constexpr` everywhere | Nearly all constructors and operators are `constexpr` |
| `auto operator<=>` | Three-way comparison for all value types |
| `std::integral auto` | Abbreviated function templates with concepts |
| `std::byteswap` | Network byte order conversion |
| `std::span` | Buffer views in `Socket::send/recv` |
| CTAD | Deduction guides for `Ip(a, b, c, d)` → `Ip<4>` |

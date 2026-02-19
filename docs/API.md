# API Reference

## Table of Contents

- [etherz (Root)](#etherz-root)
- [etherz::net::Ip\<4\>](#etherznetip4)
- [etherz::net::Ip\<6\>](#etherznetip6)
- [etherz::net::SocketAddress\<Ip\<4\>\>](#etherznetsocketaddressip4)
- [etherz::net::SocketAddress\<Ip\<6\>\>](#etherznetsocketaddressip6)
- [etherz::net::Tcp\<T\>](#etherznettcpt)
- [etherz::net::Socket\<Ip\<4\>\>](#etherznetsocketip4)
- [etherz::core::Error](#etherzcoreerror)

---

## etherz (Root)

**Header:** `#include "etherz.hpp"`

Library metadata — all functions are `consteval`.

| Function | Return | Description |
|----------|--------|-------------|
| `etherz::major()` | `uint8_t` | Major version (`0`) |
| `etherz::minor()` | `uint8_t` | Minor version (`1`) |
| `etherz::patch()` | `uint8_t` | Patch version (`0`) |
| `etherz::version()` | `string_view` | Version string `"0.1.0"` |
| `etherz::author()` | `string_view` | Author name |
| `etherz::email()` | `string_view` | Author email |
| `etherz::github()` | `string_view` | GitHub profile URL |
| `etherz::repository()` | `string_view` | Repository URL |

---

## etherz::net::Ip\<4\>

**Header:** `#include "net/internet_protocol.hpp"`

IPv4 address representation using 4 × `uint8_t`.

### Constructors

```cpp
constexpr Ip() noexcept;                                         // 0.0.0.0
constexpr Ip(uint32_t val) noexcept;                             // From 32-bit integer
constexpr Ip(integral auto a, b, c, d) noexcept;                 // From 4 octets
constexpr Ip(uint8_t (&arr)[4]) noexcept;                        // From C array
constexpr Ip(std::string_view str) noexcept;                     // Parse "a.b.c.d"
```

**CTAD:** `Ip(192, 168, 1, 1)` deduces to `Ip<4>`.

### Operators

| Operator | Signature | Description |
|----------|-----------|-------------|
| `+` | `Ip operator+(uint32_t) const` | Add offset |
| `-` | `Ip operator-(uint32_t) const` | Subtract offset |
| `+=` | `Ip& operator+=(uint32_t)` | Add offset in-place |
| `-=` | `Ip& operator-=(uint32_t)` | Subtract offset in-place |
| `++` | `Ip& operator++()` / `Ip operator++(int)` | Increment |
| `--` | `Ip& operator--()` / `Ip operator--(int)` | Decrement |
| `<=>` | `auto operator<=>(const Ip&) const` | Three-way comparison |

### Methods

| Method | Return | Description |
|--------|--------|-------------|
| `bytes()` | `const array<uint8_t, 4>&` | Raw bytes access |
| `to_uint32()` | `uint32_t` | Convert to host-order 32-bit value |
| `from_uint32(uint32_t)` | `void` | Set from host-order 32-bit value |
| `to_network()` | `uint32_t` | Convert to network byte order |
| `fill(integral auto v)` | `void` | Fill all octets with `v` |
| `display()` | `void` | Print `"IPv4: a.b.c.d\n"` |

---

## etherz::net::Ip\<6\>

**Header:** `#include "net/internet_protocol.hpp"`

IPv6 address representation using 8 × `uint16_t`.

### Constructors

```cpp
constexpr Ip() noexcept;                                         // ::
constexpr Ip(integral auto g0..g7) noexcept;                     // From 8 groups
constexpr Ip(std::string_view str) noexcept;                     // Parse "xxxx:xxxx:...:xxxx" or "::1"
```

**String parsing** supports `::` abbreviation for consecutive zero groups.

### Operators

| Operator | Description |
|----------|-------------|
| `++` / `--` | Increment/decrement (ripple-carry across all groups) |
| `<=>` | Three-way comparison |

### Methods

| Method | Return | Description |
|--------|--------|-------------|
| `bytes()` | `const array<uint16_t, 8>&` | Raw groups access |
| `fill(integral auto v)` | `void` | Fill all groups with `v` |
| `display()` | `void` | Print `"IPv6: xxxx:xxxx:...:xxxx\n"` |

---

## etherz::net::SocketAddress\<Ip\<4\>\>

**Header:** `#include "net/socket_address.hpp"`

IPv4 socket address (IP + port pair).

### Constructors

```cpp
constexpr SocketAddress() noexcept;                              // 0.0.0.0:0
constexpr SocketAddress(Ip<4> addr, uint16_t port) noexcept;     // From Ip + port
constexpr SocketAddress(string_view addr, string_view port) noexcept; // Parse strings
```

### Methods

| Method | Return | Description |
|--------|--------|-------------|
| `address()` | `const Ip<4>&` / `Ip<4>&` | Get/set IP address |
| `port()` | `uint16_t` | Get port number |
| `set_port(uint16_t)` | `void` | Set port number |
| `set_address(Ip<4>)` | `void` | Set IP address |
| `display()` | `void` | Print `"SocketAddress IPv4: a.b.c.d:port\n"` |

---

## etherz::net::SocketAddress\<Ip\<6\>\>

**Header:** `#include "net/socket_address.hpp"`

Same interface as `SocketAddress<Ip<4>>` but for IPv6 addresses.

```cpp
constexpr SocketAddress() noexcept;
constexpr SocketAddress(Ip<6> addr, uint16_t port) noexcept;
```

Display format: `SocketAddress IPv6: [xxxx:...:xxxx]:port`

---

## etherz::net::Tcp\<T\>

**Header:** `#include "net/tcp.hpp"`

TCP endpoint struct. Available for `T = Ip<4>` and `T = Ip<6>`.

### Members

| Member | Type | Description |
|--------|------|-------------|
| `address` | `T` | IP address |
| `port` | `uint16_t` | Port number |

### Constructors

```cpp
constexpr Tcp() noexcept;                   // Default (0.0.0.0:0 or :::0)
constexpr Tcp(T addr, uint16_t p) noexcept; // From address + port
```

### Methods

| Method | Description |
|--------|-------------|
| `display()` | Print `"TCP IPv4: a.b.c.d:port\n"` or `"TCP IPv6: [..]:port\n"` |
| `operator<=>` | Three-way comparison |

---

## etherz::net::Socket\<Ip\<4\>\>

**Header:** `#include "net/socket.hpp"`

RAII TCP socket wrapper for IPv4. Non-copyable, movable.

### Methods

| Method | Return | Description |
|--------|--------|-------------|
| `create()` | `core::Error` | Create the OS socket |
| `bind(addr)` | `core::Error` | Bind to a `SocketAddress<Ip<4>>` |
| `listen(backlog)` | `core::Error` | Listen for connections |
| `accept()` | `AcceptResult` | Accept incoming connection |
| `connect(addr)` | `core::Error` | Connect to remote address |
| `send(span<const uint8_t>)` | `int` | Send data (returns bytes sent or -1) |
| `recv(span<uint8_t>)` | `int` | Receive data (returns bytes received or -1) |
| `close()` | `void` | Close the socket |
| `is_open()` | `bool` | Check if socket is valid |
| `native_handle()` | `socket_t` | Get raw OS handle |

### AcceptResult

```cpp
struct AcceptResult {
    Socket client;           // Connected client socket
    SocketAddress<Ip<4>> address;  // Client address
    core::Error error;       // Error code
};
```

---

## etherz::core::Error

**Header:** `#include "core/error.hpp"`

### Error Codes

| Code | Value | Description |
|------|-------|-------------|
| `None` | 0 | No error |
| `InvalidAddress` | 1 | Invalid address format |
| `InvalidPort` | 2 | Invalid port number |
| `SocketCreationFailed` | 3 | Failed to create socket |
| `BindFailed` | 4 | Failed to bind socket |
| `ListenFailed` | 5 | Failed to listen |
| `AcceptFailed` | 6 | Failed to accept connection |
| `ConnectFailed` | 7 | Failed to connect |
| `ConnectionRefused` | 8 | Connection refused |
| `ConnectionReset` | 9 | Connection reset by peer |
| `SendFailed` | 10 | Failed to send data |
| `ReceiveFailed` | 11 | Failed to receive data |
| `Timeout` | 12 | Operation timed out |
| `AddressInUse` | 13 | Address already in use |
| `AddressNotAvailable` | 14 | Address not available |
| `NetworkUnreachable` | 15 | Network unreachable |
| `HostUnreachable` | 16 | Host unreachable |
| `AlreadyConnected` | 17 | Already connected |
| `NotConnected` | 18 | Not connected |
| `SocketClosed` | 19 | Socket is closed |
| `Unknown` | 20 | Unknown error |

### Helper Functions

| Function | Return | Description |
|----------|--------|-------------|
| `error_message(Error)` | `string_view` | Human-readable error string |
| `is_ok(Error)` | `bool` | `true` if `Error::None` |
| `is_error(Error)` | `bool` | `true` if not `Error::None` |

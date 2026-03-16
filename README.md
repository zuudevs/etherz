<div align="center">

# Etherz

**Modern C++23 Header-Only Networking Library**

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE.md)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square)]()
[![Version](https://img.shields.io/badge/Version-1.0.1-orange?style=flat-square)]()
[![Build](https://img.shields.io/badge/Build-Passing-brightgreen?style=flat-square)]()

*A lightweight, type-safe, and modern networking library built with C++23 features.*

</div>

---

## Features

- **IPv4 & IPv6** — Full IP address representation with parsing, arithmetic, and comparison
- **TCP & UDP Sockets** — RAII socket wrappers with platform abstraction
- **Async I/O** — Poll, event loop, and async socket operations
- **HTTP/1.1** — Client (GET/POST + HTTPS), server with routing, request/response parsing
- **WebSocket** — Frame encode/decode, handshake helpers
- **TLS/SSL** — SChannel-based `TlsSocket<T>` wrapper with certificate management
- **DNS** — Hostname resolution, reverse lookup, IPv4/IPv6
- **Network Interfaces** — Enumerate local NICs with MAC, IP, status
- **Subnet/CIDR** — Parse, contains, mask, broadcast, host counting
- **ICMP Ping** — Lightweight ping utility
- **Header-Only** — Just `#include` and go, no linking required
- **Modern C++23** — Uses concepts, `<=>` operator, `std::print`, `constexpr`, CTAD, and more

## Requirements

| Requirement | Minimum |
|-------------|---------|
| C++ Standard | C++23 |
| CMake | 3.20+ |
| Compiler | MSVC 19.38+, Clang 17+, GCC 14+ |

## Quick Start

```cpp
#include "net/internet_protocol.hpp"
#include "net/dns.hpp"
#include "net/subnet.hpp"
#include "protocol/http_client.hpp"
#include <print>

namespace etn = etherz::net;
namespace etp = etherz::protocol;

int main() {
    // IPv4
    auto ip = etn::Ip(192, 168, 1, 1);
    std::print("IPv4: {}\n", ip);  // IPv4: 192.168.1.1

    // DNS
    auto dns = etn::Dns::resolve("localhost");
    // → 127.0.0.1 + ::1

    // Subnet
    auto subnet = etn::Subnet<etn::Ip<4>>::parse("192.168.1.0/24");
    std::print("Contains: {}\n", subnet.contains(ip));  // true

    // HTTP GET
    if (auto res = etp::HttpClient::get("http://example.com")) {
        std::print("Status: {}\n", static_cast<int>(res->status));
    } else {
        std::print("Error: {}\n", res.error());
    }

    // Ping
    auto result = etn::ping(etn::Ip<4>(127, 0, 0, 1));
    // → rtt=0ms, ttl=128

    return 0;
}
```

## Build

```bash
# Basic build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# With tests and examples
cmake -S . -B build -DETHERZ_BUILD_TESTS=ON -DETHERZ_BUILD_EXAMPLES=ON
cmake --build build
./bin/etherz_tests     # Run unit tests
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## Documentation

| Document | Description |
|----------|-------------|
| [API.md](docs/API.md) | Complete API reference |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Architecture overview with diagrams |
| [QUICKSTART.md](docs/QUICKSTART.md) | Quick start guide with examples |
| [ROADMAP.md](docs/ROADMAP.md) | Future plans and milestones |
| [CHANGELOG.md](docs/CHANGELOG.md) | Version history |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines |
| [SECURITY.md](SECURITY.md) | Security policy |

## Project Structure

```
etherz/
├── include/
│   ├── etherz.hpp                  # Library metadata (version, author)
│   ├── core/
│   │   └── error.hpp               # Error types and utilities
│   ├── net/
│   │   ├── internet_protocol.hpp   # Ip<4> and Ip<6> classes
│   │   ├── socket_address.hpp      # SocketAddress<T> templates
│   │   ├── tcp.hpp / udp.hpp       # Endpoint structs
│   │   ├── socket.hpp              # RAII Socket<T> wrapper
│   │   ├── udp_socket.hpp          # UDP socket
│   │   ├── dns.hpp                 # DNS resolution
│   │   ├── subnet.hpp              # Subnet/CIDR utilities
│   │   ├── network_interface.hpp   # NIC enumeration
│   │   └── ping.hpp                # ICMP ping
│   ├── async/
│   │   ├── poll.hpp                # Platform poll wrapper
│   │   ├── event_loop.hpp          # Callback event loop
│   │   └── async_socket.hpp        # Async socket ops
│   ├── protocol/
│   │   ├── url.hpp                 # URL parser
│   │   ├── http.hpp                # HTTP request/response
│   │   ├── http_client.hpp         # HTTP + HTTPS client
│   │   ├── http_server.hpp         # HTTP server with routing
│   │   └── websocket.hpp           # WebSocket frames
│   └── security/
│       ├── tls_context.hpp         # TLS configuration
│       ├── tls_socket.hpp          # SChannel TLS wrapper
│       └── certificate.hpp         # X.509 certificate info
├── src/main.cpp                    # Demo application
├── tests/                          # Unit test suite
├── examples/                       # Example programs
├── docs/                           # Documentation
├── cmake/                          # Package config
└── CMakeLists.txt                  # Build configuration
```

## License

This project is licensed under the MIT License — see [LICENSE.md](LICENSE.md) for details.

## Author

**zuudevs** — [GitHub](https://github.com/zuudevs) · [Email](mailto:zuudevs@gmail.com)

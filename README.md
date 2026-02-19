<div align="center">

# ⚡ Etherz

**Modern C++23 Header-Only Networking Library**

[![C++ Standard](https://img.shields.io/badge/C%2B%2B-23-blue?style=flat-square&logo=cplusplus)](https://isocpp.org/)
[![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)](LICENSE.md)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey?style=flat-square)]()
[![Version](https://img.shields.io/badge/Version-0.1.0-orange?style=flat-square)]()

*A lightweight, type-safe, and modern networking library built with C++23 features.*

</div>

---

## ✨ Features

- **IPv4 & IPv6** — Full IP address representation with parsing, arithmetic, and comparison
- **Socket Addresses** — Type-safe `SocketAddress<Ip<4>>` and `SocketAddress<Ip<6>>`
- **TCP Endpoints** — Clean `Tcp<Ip<4>>` and `Tcp<Ip<6>>` endpoint structs
- **RAII Sockets** — Platform-aware TCP socket wrapper with automatic cleanup
- **Error Handling** — Comprehensive error enum with human-readable messages
- **Header-Only** — Just `#include` and go, no linking required
- **Modern C++23** — Uses concepts, `<=>` operator, `std::print`, `constexpr`, CTAD, and more
- **Cross-Platform** — Windows (WinSock2) and POSIX (Linux/macOS) socket support

## 📋 Requirements

| Requirement | Minimum |
|-------------|---------|
| C++ Standard | C++23 |
| CMake | 3.20+ |
| Compiler | MSVC 19.38+, Clang 17+, GCC 14+ |

## 🚀 Quick Start

```cpp
#include "net/internet_protocol.hpp"
#include "net/socket_address.hpp"
#include "net/tcp.hpp"

namespace etn = etherz::net;

int main() {
    // IPv4
    auto ip = etn::Ip(192, 168, 1, 1);
    auto ip_parsed = etn::Ip<4>{"10.0.0.1"};
    ip.display();  // IPv4: 192.168.1.1

    // IPv6 with :: abbreviation
    auto ip6 = etn::Ip<6>{"2001:db8::1"};
    ip6.display(); // IPv6: 2001:0db8:0000:0000:0000:0000:0000:0001

    // Socket Address
    auto addr = etn::SocketAddress<etn::Ip<4>>(ip, 8080);
    addr.display(); // SocketAddress IPv4: 192.168.1.1:8080

    // TCP Endpoint
    auto tcp = etn::Tcp<etn::Ip<4>>(ip, 80);
    tcp.display(); // TCP IPv4: 192.168.1.1:80

    return 0;
}
```

## 🏗️ Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./bin/etherz     # or .\bin\etherz.exe on Windows
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## 📖 Documentation

| Document | Description |
|----------|-------------|
| [QUICKSTART.md](docs/QUICKSTART.md) | Getting started guide with examples |
| [API.md](docs/API.md) | Complete API reference |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design patterns and architecture overview |
| [ROADMAP.md](docs/ROADMAP.md) | Future plans and milestones |
| [CHANGELOG.md](docs/CHANGELOG.md) | Version history |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines |

## 📁 Project Structure

```
etherz/
├── include/
│   ├── etherz.hpp                  # Library metadata (version, author)
│   ├── core/
│   │   └── error.hpp               # Error types and utilities
│   └── net/
│       ├── internet_protocol.hpp   # Ip<4> and Ip<6> classes
│       ├── socket_address.hpp      # SocketAddress<T> templates
│       ├── tcp.hpp                 # Tcp<T> endpoint structs
│       └── socket.hpp              # RAII Socket<T> wrapper
├── src/
│   └── main.cpp                    # Demo application
├── docs/                           # Documentation
├── tests/                          # Unit tests (planned)
├── examples/                       # Example programs (planned)
└── CMakeLists.txt                  # Build configuration
```

## 📄 License

This project is licensed under the MIT License — see [LICENSE.md](LICENSE.md) for details.

## 👤 Author

**zuudevs** — [GitHub](https://github.com/zuudevs) · [Email](mailto:zuudevs@gmail.com)

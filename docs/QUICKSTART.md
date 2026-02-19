# Quick Start Guide

## Installation

Etherz is **header-only** — just add the `include/` directory to your project.

### Option 1: Copy Headers

```bash
# Copy the include directory into your project
cp -r etherz/include/ your-project/third-party/etherz/
```

Then add to your CMake:

```cmake
target_include_directories(your_target PRIVATE "third-party/etherz")
```

### Option 2: Git Submodule

```bash
git submodule add https://github.com/zuudevs/etherz.git external/etherz
```

```cmake
target_include_directories(your_target PRIVATE "external/etherz/include")
```

---

## Basic Usage

### IPv4 Addresses

```cpp
#include "net/internet_protocol.hpp"
namespace etn = etherz::net;

// Construction
auto ip1 = etn::Ip(192, 168, 1, 1);        // CTAD → Ip<4>
auto ip2 = etn::Ip<4>{"10.0.0.1"};          // From string
auto ip3 = etn::Ip<4>(0xC0A80101);           // From uint32_t

// Arithmetic
auto next = ip1 + 1;           // 192.168.1.2
++ip1;                         // 192.168.1.2
ip1 += 10;                     // 192.168.1.12

// Comparison
if (ip1 == ip2) { /* ... */ }
if (ip1 < ip2) { /* ... */ }

// Network byte order (for socket APIs)
uint32_t net_order = ip1.to_network();

// Display
ip1.display();  // → "IPv4: 192.168.1.12"
```

### IPv6 Addresses

```cpp
// Full notation
auto ip6 = etn::Ip<6>(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1);

// String parsing with :: abbreviation
auto loopback = etn::Ip<6>{"::1"};
auto link_local = etn::Ip<6>{"fe80::1"};
auto full = etn::Ip<6>{"2001:0db8:85a3:0000:0000:8a2e:0370:7334"};

// Increment/decrement
++loopback;  // ::2

loopback.display();  // → "IPv6: 0000:0000:0000:0000:0000:0000:0000:0002"
```

### Socket Addresses

```cpp
#include "net/socket_address.hpp"

// From Ip + port
auto addr = etn::SocketAddress<etn::Ip<4>>(
    etn::Ip(127, 0, 0, 1), 8080
);

// From strings
auto addr2 = etn::SocketAddress<etn::Ip<4>>("0.0.0.0", "3000");

// Accessors
auto ip = addr.address();
auto port = addr.port();
addr.set_port(9090);

addr.display();  // → "SocketAddress IPv4: 127.0.0.1:9090"

// IPv6 socket address
auto addr6 = etn::SocketAddress<etn::Ip<6>>(
    etn::Ip<6>{"::1"}, 443
);
addr6.display();  // → "SocketAddress IPv6: [0000:...:0001]:443"
```

### TCP Endpoints

```cpp
#include "net/tcp.hpp"

auto tcp = etn::Tcp<etn::Ip<4>>(etn::Ip(10, 0, 0, 1), 80);
tcp.display();  // → "TCP IPv4: 10.0.0.1:80"

auto tcp6 = etn::Tcp<etn::Ip<6>>(etn::Ip<6>{"::1"}, 443);
tcp6.display(); // → "TCP IPv6: [0000:...:0001]:443"
```

### Error Handling

```cpp
#include "core/error.hpp"
namespace etc = etherz::core;

auto err = etc::Error::None;

if (etc::is_ok(err)) {
    // Success path
}

if (etc::is_error(err)) {
    std::print("Error: {}\n", etc::error_message(err));
}
```

### TCP Socket (Simple Server)

```cpp
#include "net/socket.hpp"

using etn::Socket;
using etn::SocketAddress;
using etn::Ip;

Socket<Ip<4>> server;

// Create socket
auto err = server.create();
if (etc::is_error(err)) return 1;

// Bind to 0.0.0.0:8080
err = server.bind(SocketAddress<Ip<4>>(Ip(0, 0, 0, 0), 8080));
if (etc::is_error(err)) return 1;

// Listen
err = server.listen();
if (etc::is_error(err)) return 1;

// Accept a client
auto [client, client_addr, accept_err] = server.accept();
if (etc::is_ok(accept_err)) {
    client_addr.display();

    // Receive data
    std::array<uint8_t, 1024> buffer{};
    int n = client.recv(buffer);

    // Send response
    std::string_view response = "Hello from Etherz!";
    client.send({reinterpret_cast<const uint8_t*>(response.data()), response.size()});
}
// Sockets auto-close on destruction (RAII)
```

---

## Build & Run the Demo

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./bin/etherz          # Linux/macOS
.\bin\etherz.exe      # Windows
```

See [BUILD.md](../BUILD.md) for more options.

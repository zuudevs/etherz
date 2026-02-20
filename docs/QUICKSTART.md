# Quick Start Guide

## Installation

Etherz is **header-only** — just add the `include/` directory to your project.

### Option 1: Copy Headers

```bash
cp -r etherz/include/ your-project/third-party/etherz/
```

```cmake
target_include_directories(your_target PRIVATE "third-party/etherz")
# Windows only:
target_link_libraries(your_target PRIVATE ws2_32 secur32 iphlpapi)
```

### Option 2: Git Submodule

```bash
git submodule add https://github.com/zuudevs/etherz.git external/etherz
```

```cmake
target_include_directories(your_target PRIVATE "external/etherz/include")
```

### Option 3: CMake find_package

```cmake
list(APPEND CMAKE_PREFIX_PATH "/path/to/etherz/cmake")
find_package(etherz REQUIRED)
target_link_libraries(your_target PRIVATE etherz::etherz)
```

---

## Basic Usage

### IPv4 / IPv6

```cpp
#include "net/internet_protocol.hpp"
namespace etn = etherz::net;

// IPv4
auto ip = etn::Ip(192, 168, 1, 1);        // CTAD → Ip<4>
auto ip2 = etn::Ip<4>{"10.0.0.1"};        // From string
auto next = ip + 1;                         // 192.168.1.2

// IPv6
auto ip6 = etn::Ip<6>{"2001:db8::1"};
auto loopback = etn::Ip<6>{"::1"};
```

### TCP Socket (Server)

```cpp
#include "net/socket.hpp"

etn::Socket<etn::Ip<4>> server;
server.create();
server.bind(etn::SocketAddress<etn::Ip<4>>(etn::Ip(0,0,0,0), 8080));
server.listen(5);

auto result = server.accept();
auto client = result.take_client();

uint8_t buf[1024]{};
int n = client.recv(std::span<uint8_t>(buf, sizeof(buf)));
client.send(std::span<const uint8_t>(buf, n));
// Sockets auto-close on destruction (RAII)
```

### UDP Socket

```cpp
#include "net/udp_socket.hpp"

// IPv4 UDP
etn::UdpSocket<etn::Ip<4>> udp;
udp.create();
udp.bind(etn::SocketAddress<etn::Ip<4>>(etn::Ip(0,0,0,0), 9000));

uint8_t buf[1024]{};
auto result = udp.recv_from(std::span<uint8_t>(buf, sizeof(buf)));
if (result.bytes > 0) {
    udp.send_to(std::span<const uint8_t>(buf, result.bytes), result.sender);
}

// IPv6 UDP
etn::UdpSocket<etn::Ip<6>> udp6;
udp6.create();
udp6.bind(etn::SocketAddress<etn::Ip<6>>(etn::Ip<6>{"::"}, 9000));
```

### DNS Resolution

```cpp
#include "net/dns.hpp"

auto result = etn::Dns::resolve("example.com");
for (auto& ip : result.ipv4_addresses) ip.display();
for (auto& ip : result.ipv6_addresses) ip.display();

auto hostname = etn::Dns::reverse(etn::Ip<4>(8, 8, 8, 8));
```

### Subnet / CIDR

```cpp
#include "net/subnet.hpp"

auto subnet = etn::Subnet<etn::Ip<4>>::parse("192.168.1.0/24");
subnet.contains(etn::Ip<4>(192, 168, 1, 50));  // true
subnet.broadcast().display();                    // 192.168.1.255
subnet.host_count();                             // 254
```

### ICMP Ping

```cpp
#include "net/ping.hpp"

auto result = etn::ping(etn::Ip<4>(127, 0, 0, 1));
if (result.status == etn::PingStatus::Success) {
    std::print("rtt={}ms ttl={}\n", result.rtt_ms, result.ttl);
}
```

### Network Interfaces

```cpp
#include "net/network_interface.hpp"

for (auto& iface : etn::list_interfaces()) {
    std::print("{} — {}\n", iface.name, iface.is_up ? "UP" : "DOWN");
}
```

---

## HTTP Client

```cpp
#include "protocol/http_client.hpp"
namespace etp = etherz::protocol;

etp::HttpClient client;

// HTTP GET — hostnames are automatically DNS-resolved
auto url = etp::Url::parse("http://example.com");
auto result = client.get(url);
std::print("Status: {}\n", static_cast<int>(result.response.status));
std::print("Body: {}\n", result.response.body);

// HTTPS (auto-detected by scheme)
auto url2 = etp::Url::parse("https://example.com");
auto result2 = client.get(url2);

// POST with body (Content-Length auto-included)
auto url3 = etp::Url::parse("http://api.example.com/data");
auto result3 = client.post(url3, R"({"key":"value"})");
```

### HTTP Server

```cpp
#include "protocol/http_server.hpp"

etp::HttpServer server;
server.route(etp::HttpMethod::Get, "/", [](const auto& req) {
    etp::HttpResponse resp;
    resp.body = "<h1>Hello from Etherz!</h1>";
    resp.headers.set("Content-Type", "text/html");
    return resp;
});
server.listen(etn::SocketAddress<etn::Ip<4>>(etn::Ip(0,0,0,0), 8080));
```

### WebSocket

```cpp
#include "protocol/websocket.hpp"

etp::WsFrame frame;
frame.set_text("Hello WebSocket!");
auto encoded = etp::ws_encode_frame(frame);
auto decoded = etp::ws_decode_frame(encoded);
```

---

## TLS / HTTPS

```cpp
#include "security/tls_context.hpp"
#include "security/tls_socket.hpp"
#include "security/certificate.hpp"

namespace ets = etherz::security;

// TLS Context
auto ctx = ets::TlsContext::client();

// Self-signed certificate info (for testing)
auto cert = ets::make_self_signed_info("localhost");
cert.display();
```

---

## Build & Run

```bash
# Basic
cmake -S . -B build && cmake --build build
./bin/etherz

# With tests and examples
cmake -S . -B build -DETHERZ_BUILD_TESTS=ON -DETHERZ_BUILD_EXAMPLES=ON
cmake --build build
./bin/etherz_tests

# Examples
./bin/echo_server
./bin/dns_lookup google.com
./bin/ping_tool 8.8.8.8
./bin/subnet_calc 192.168.1.0/24
```

See [BUILD.md](../BUILD.md) for more options.

# Etherz API Reference

> Auto-generated detail available via `doxygen docs/Doxyfile`

## Namespaces

| Namespace | Description |
|-----------|-------------|
| `etherz::core` | Error handling, platform abstractions |
| `etherz::net` | IP, sockets, DNS, subnet, ping, interfaces |
| `etherz::async` | Poll, event loop, async socket |
| `etherz::protocol` | URL, HTTP, WebSocket |
| `etherz::security` | TLS context, TLS socket, certificates |

---

## Core (`core/`)

### `error.hpp`
- `enum class Error` — Unified error codes
- `error_message(Error)` — Human-readable error string
- `from_platform_error(int)` — Platform code → Error mapping

---

## Net (`net/`)

### `internet_protocol.hpp`
- `Ip<4>` — IPv4 address (construct, parse, arithmetic, compare)
- `Ip<6>` — IPv6 address (construct, parse, compare)

### `socket.hpp`
- `Socket<Ip<V>>` — TCP socket (create, bind, listen, accept, connect, send, recv)

### `udp_socket.hpp`
- `UdpSocket<Ip<V>>` — UDP socket (sendto, recvfrom)

### `dns.hpp`
- `Dns::resolve(hostname)` → `DnsResult` (IPv4 + IPv6)
- `Dns::reverse(Ip<4>)` → hostname string

### `subnet.hpp`
- `Subnet<Ip<4>>::parse("cidr")` — CIDR parser
- `contains(ip)`, `mask()`, `network()`, `broadcast()`, `host_count()`

### `network_interface.hpp`
- `list_interfaces()` → `vector<NetworkInterface>`

### `ping.hpp`
- `ping(Ip<4>, timeout)` → `PingResult`

---

## Async (`async/`)

### `poll.hpp`
- `Poll` — Platform poll wrapper (add, remove, wait)

### `event_loop.hpp`
- `EventLoop` — Callback-driven event loop

### `async_socket.hpp`
- `AsyncSocket` — Non-blocking socket with async ops

---

## Protocol (`protocol/`)

### `url.hpp`
- `Url::parse(str)` — Full URL parser

### `http.hpp`
- `HttpRequest` / `HttpResponse` — Serialize + parse
- `HttpHeaders` — Case-insensitive header map

### `http_client.hpp`
- `HttpClient::get(url)` / `post(url, body)` — HTTP + HTTPS

### `http_server.hpp`
- `HttpServer` — Routing-based HTTP server

### `websocket.hpp`
- `WsFrame` — Frame encode/decode

---

## Security (`security/`)

### `tls_context.hpp`
- `TlsContext` — TLS configuration (method, verify mode, role)

### `tls_socket.hpp`
- `TlsSocket<T>` — Encrypted socket wrapper (SChannel)

### `certificate.hpp`
- `CertInfo` — Certificate information struct

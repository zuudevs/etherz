# Etherz API Reference

> Auto-generated detail available via `doxygen docs/Doxyfile`

## Namespaces

| `etherz::core` | Error handling, platform abstractions |
| `etherz::net` | IP, sockets, DNS, subnet, ping, interfaces, UPnP, STUN |
| `etherz::async` | Poll, event loop, async socket, thread pool, parallel acceptor |
| `etherz::protocol` | URL, HTTP, WebSocket, REST, MQTT, gRPC |
| `etherz::security` | TLS context, TLS socket, certificates |

---

## Core (`core/`)

### `error.hpp`
- `enum class Error` — Unified error codes (includes `UpnpError`, `StunError`)
- `error_message(Error)` — Human-readable error string
- `from_platform_error(int)` — Platform code → Error mapping

---

## Net (`net/`)

### `internet_protocol.hpp`
- `Ip<4>` — IPv4 address (construct, parse, arithmetic, compare)
- `Ip<6>` — IPv6 address (construct, parse, compare)

### `socket.hpp`
- `Socket<Ip<V>>` — TCP socket (create, bind, listen, accept -> `expected`, connect, send, recv)

### `udp_socket.hpp`
- `UdpSocket<Ip<4>>` — UDP IPv4 socket (sendto, recvfrom)
- `UdpSocket<Ip<6>>` — UDP IPv6 socket (sendto, recvfrom)

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

### `upnp.hpp`
- `UpnpClient::discover()` — SSDP M-SEARCH for IGD devices
- `UpnpClient::fetch_control_url()` — Fetch SOAP control URL
- `UpnpClient::add_port_mapping()` / `delete_port_mapping()` — Port mapping management
- `UpnpClient::get_external_ip()` → `expected<string, Error>`

### `stun.hpp`
- `StunClient::query(server)` → `StunResult` (public IP, port, NAT type)
- `StunClient::get_public_ip()` → `expected<Ip<4>, Error>`
- `enum class NatType` — Open, FullCone, Restricted, PortRestricted, Symmetric

---

## Async (`async/`)

### `poll.hpp`
- `poll()` — Platform poll wrapper (`WSAPoll` / `::poll`) with `native_pollfd` abstraction

### `event_loop.hpp`
- `EventLoop` — Callback-driven event loop with snapshot-based dispatch

### `async_socket.hpp`
- `AsyncSocket` — Non-blocking socket with async ops

### `thread_pool.hpp`
- `ThreadPool(num_threads)` — Worker pool with task queue
- `submit(callable, args...)` → `std::future<R>` — Submit task with future result
- `shutdown()` / `is_stopped()` / `pending_tasks()` / `thread_count()`

### `parallel_socket.hpp`
- `ParallelAcceptor<T>::start(port, handler)` — Multi-threaded TCP acceptor
- `stop()` / `is_running()` / `active_connections()` / `total_accepted()`

---

## Protocol (`protocol/`)

### `url.hpp`
- `Url::parse(str)` — Full URL parser

### `http.hpp`
- `HttpRequest` / `HttpResponse` — Serialize + parse
- `HttpHeaders` — Case-insensitive header map

### `http_client.hpp`
- `HttpClient::get(url)` / `post(url, body)` → `std::expected<HttpResponse, Error>`

### `http_server.hpp`
- `HttpServer` — Routing-based HTTP server (multi-read request handling)

### `websocket.hpp`
- `WsFrame` — Frame encode/decode

### `ws_client.hpp`
- `WsClient::connect(url)` — HTTP upgrade + WebSocket handshake
- `send_text()` / `send_binary()` / `send_ping()` — Message sending
- `recv()` → `expected<WsMessage, Error>` — Auto ping/pong, fragmentation

### `ws_server.hpp`
- `WsServer::listen(port)` / `poll()` / `stop()` — WebSocket server lifecycle
- `on_connect()` / `on_message()` / `on_disconnect()` — Per-connection callbacks
- `broadcast(text)` — Send to all connected clients

### `rest_client.hpp`
- `RestClient::get()` / `post()` / `put()` / `patch()` / `del()` → `expected<RestResponse, Error>`
- `post_json()` / `put_json()` / `patch_json()` — JSON body via zuu-json
- `RestAuth::bearer()` / `basic()` — Authentication helpers
- `RestResponse::parse_json(arena)` — Parse response body as JSON

---

## Security (`security/`)

### `tls_context.hpp`
- `TlsContext` — TLS configuration (method, verify mode, role)

### `tls_socket.hpp`
- `TlsSocket<T>` — Encrypted socket wrapper (SChannel) with partial record handling

### `certificate.hpp`
- `CertInfo` — Certificate information struct

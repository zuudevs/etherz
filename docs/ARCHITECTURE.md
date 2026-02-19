# Architecture

## Overview

Etherz is a **header-only** C++23 networking library organized into five modules:

```mermaid
graph TB
    subgraph "etherz"
        direction TB
        E["etherz.hpp<br/>Version & Metadata"]

        subgraph core["core/"]
            C1["error.hpp"]
        end

        subgraph net["net/"]
            N1["internet_protocol.hpp"]
            N2["socket_address.hpp"]
            N3["tcp.hpp · udp.hpp"]
            N4["socket.hpp"]
            N5["udp_socket.hpp"]
            N6["dns.hpp"]
            N7["subnet.hpp"]
            N8["network_interface.hpp"]
            N9["ping.hpp"]
        end

        subgraph async["async/"]
            A1["poll.hpp"]
            A2["event_loop.hpp"]
            A3["async_socket.hpp"]
        end

        subgraph protocol["protocol/"]
            P1["url.hpp"]
            P2["http.hpp"]
            P3["http_client.hpp"]
            P4["http_server.hpp"]
            P5["websocket.hpp"]
        end

        subgraph security["security/"]
            S1["tls_context.hpp"]
            S2["tls_socket.hpp"]
            S3["certificate.hpp"]
        end
    end

    style core fill:#2d4a7a,stroke:#4a90d9,color:#fff
    style net fill:#2a6041,stroke:#4bc07a,color:#fff
    style async fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style protocol fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style security fill:#7a2d2d,stroke:#d94a4a,color:#fff
```

---

## Module Dependency Graph

```mermaid
graph LR
    subgraph "Dependency Flow"
        direction LR
        CORE["core::Error"]
        IP["net::Ip"]
        SA["net::SocketAddress"]
        TCP["net::Tcp"]
        SOCK["net::Socket"]
        UDP["net::UdpSocket"]
        DNS["net::Dns"]
        SUB["net::Subnet"]
        NIF["net::NetworkInterface"]
        PING["net::ping"]
        POLL["async::Poll"]
        EL["async::EventLoop"]
        AS["async::AsyncSocket"]
        URL["protocol::Url"]
        HTTP["protocol::Http"]
        HC["protocol::HttpClient"]
        HS["protocol::HttpServer"]
        WS["protocol::WebSocket"]
        TLS_CTX["security::TlsContext"]
        TLS_SOCK["security::TlsSocket"]
        CERT["security::Certificate"]

        IP --> SA
        IP --> TCP
        IP --> SOCK
        IP --> UDP
        IP --> DNS
        IP --> SUB
        IP --> NIF
        IP --> PING
        SA --> SOCK
        SA --> UDP
        CORE --> SOCK
        CORE --> UDP
        SOCK --> AS
        SOCK --> HC
        SOCK --> HS
        POLL --> EL
        EL --> AS
        URL --> HC
        HTTP --> HC
        HTTP --> HS
        SOCK --> TLS_SOCK
        TLS_CTX --> TLS_SOCK
        TLS_SOCK --> HC
        CERT --> TLS_SOCK
    end

    style CORE fill:#2d4a7a,stroke:#4a90d9,color:#fff
    style IP fill:#2a6041,stroke:#4bc07a,color:#fff
    style SA fill:#2a6041,stroke:#4bc07a,color:#fff
    style TCP fill:#2a6041,stroke:#4bc07a,color:#fff
    style SOCK fill:#2a6041,stroke:#4bc07a,color:#fff
    style UDP fill:#2a6041,stroke:#4bc07a,color:#fff
    style DNS fill:#2a6041,stroke:#4bc07a,color:#fff
    style SUB fill:#2a6041,stroke:#4bc07a,color:#fff
    style NIF fill:#2a6041,stroke:#4bc07a,color:#fff
    style PING fill:#2a6041,stroke:#4bc07a,color:#fff
    style POLL fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style EL fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style AS fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style URL fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style HTTP fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style HC fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style HS fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style WS fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style TLS_CTX fill:#7a2d2d,stroke:#d94a4a,color:#fff
    style TLS_SOCK fill:#7a2d2d,stroke:#d94a4a,color:#fff
    style CERT fill:#7a2d2d,stroke:#d94a4a,color:#fff
```

---

## Class Hierarchy

```mermaid
classDiagram
    class IpBase~Derived, T, N~ {
        #array~T, N~ m_address
        +bytes() const array~T, N~
        +operator<=>() auto
    }

    class Ip4["Ip&lt;4&gt;"] {
        +Ip(a, b, c, d)
        +Ip(string_view)
        +Ip(uint32_t)
        +to_uint32() uint32_t
        +from_uint32(uint32_t) void
        +operator+(uint32_t) Ip4
        +display() void
    }

    class Ip6["Ip&lt;6&gt;"] {
        +Ip(8x uint16_t)
        +Ip(string_view)
        +operator+(uint16_t) Ip6
        +display() void
    }

    class SocketAddress~T~ {
        -T m_address
        -uint16_t m_port
        +address() T
        +port() uint16_t
        +to_sockaddr_in() sockaddr_in
    }

    class Socket~T~ {
        -socket_t fd_
        +create() Error
        +bind(SocketAddress) Error
        +listen(int) Error
        +accept() AcceptResult
        +connect(SocketAddress) Error
        +send(span) int
        +recv(span) int
        +close() void
        +shutdown(ShutdownMode) Error
    }

    class TlsSocket~T~ {
        -T inner_socket_
        -CredentialGuard cred_
        -ContextGuard sec_ctx_
        +handshake(hostname) Error
        +send(span) int
        +recv(span) int
        +close() void
    }

    class HttpClient {
        +get(url) Result
        +post(url, body) Result
        -send_plain(url) Result
        -send_secure(url) Result
    }

    class HttpServer {
        -Socket listener_
        -route_map routes_
        +route(method, path, handler) void
        +listen(addr) Error
        +accept_one() Error
    }

    IpBase <|-- Ip4 : CRTP
    IpBase <|-- Ip6 : CRTP
    Socket --* SocketAddress : uses
    Socket --* Ip4 : parameterized
    Socket --* Ip6 : parameterized
    TlsSocket --* Socket : wraps
    HttpClient --> Socket : uses
    HttpClient --> TlsSocket : uses
    HttpServer --> Socket : uses
```

---

## TCP Connection Lifecycle

```mermaid
sequenceDiagram
    participant App as Application
    participant Sock as Socket~Ip4~
    participant OS as OS Kernel

    App->>Sock: create()
    Sock->>OS: socket(AF_INET, SOCK_STREAM)
    OS-->>Sock: fd

    App->>Sock: bind(addr)
    Sock->>OS: bind(fd, sockaddr)
    OS-->>Sock: OK

    App->>Sock: listen(backlog)
    Sock->>OS: listen(fd, backlog)
    OS-->>Sock: OK

    rect rgb(40, 80, 40)
        Note over App,OS: Accept Loop
        App->>Sock: accept()
        Sock->>OS: accept(fd)
        OS-->>Sock: client_fd + address
        Sock-->>App: AcceptResult
    end

    rect rgb(40, 40, 80)
        Note over App,OS: Data Transfer
        App->>Sock: recv(buffer)
        Sock->>OS: recv(client_fd, buf)
        OS-->>Sock: bytes_read

        App->>Sock: send(data)
        Sock->>OS: send(client_fd, data)
        OS-->>Sock: bytes_sent
    end

    App->>Sock: close()
    Sock->>OS: closesocket(fd)
    Note over Sock: RAII destructor also calls close
```

---

## TLS Handshake Flow

```mermaid
sequenceDiagram
    participant App as Application
    participant TLS as TlsSocket
    participant SCH as SChannel
    participant TCP as Socket
    participant Srv as Remote Server

    App->>TLS: handshake(hostname)
    TLS->>SCH: AcquireCredentialsHandle()
    SCH-->>TLS: CredHandle

    loop Handshake Loop
        TLS->>SCH: InitializeSecurityContext()
        SCH-->>TLS: output token + status

        alt SEC_I_CONTINUE_NEEDED
            TLS->>TCP: send(token)
            TCP->>Srv: token bytes
            Srv-->>TCP: server token
            TCP-->>TLS: recv(response)
        else SEC_E_OK
            Note over TLS: Handshake complete
        else Error
            TLS-->>App: Error::HandshakeFailed
        end
    end

    TLS->>SCH: QueryContextAttributes(STREAM_SIZES)
    SCH-->>TLS: header/trailer/max_message sizes
    TLS-->>App: Error::None

    rect rgb(40, 40, 80)
        Note over App,Srv: Encrypted Communication
        App->>TLS: send(plaintext)
        TLS->>SCH: EncryptMessage()
        TLS->>TCP: send(ciphertext)

        TCP-->>TLS: recv(ciphertext)
        TLS->>SCH: DecryptMessage()
        TLS-->>App: plaintext
    end
```

---

## HTTP Request Flow

```mermaid
flowchart TD
    START([HttpClient::get/post]) --> PARSE[Parse URL]
    PARSE --> SCHEME{Scheme?}

    SCHEME -->|http://| PLAIN[Create Socket]
    SCHEME -->|https://| SECURE[Create Socket + TlsSocket]

    PLAIN --> CONNECT[connect to host:port]
    SECURE --> CONNECT2[connect to host:443]
    CONNECT2 --> HANDSHAKE[TLS handshake]

    CONNECT --> SERIALIZE[Serialize HttpRequest]
    HANDSHAKE --> SERIALIZE

    SERIALIZE --> SEND[send raw bytes]
    SEND --> RECV[recv response bytes]
    RECV --> PARSE_RESP[http_parser::parse_response]
    PARSE_RESP --> RESULT([Return HttpResponse + Error])

    style START fill:#2d4a7a,stroke:#4a90d9,color:#fff
    style RESULT fill:#2a6041,stroke:#4bc07a,color:#fff
    style SECURE fill:#7a2d2d,stroke:#d94a4a,color:#fff
    style HANDSHAKE fill:#7a2d2d,stroke:#d94a4a,color:#fff
```

---

## Async Event Loop

```mermaid
flowchart TD
    INIT([EventLoop::run]) --> POLL[Poll::wait for events]
    POLL --> HAS{Events ready?}

    HAS -->|Yes| DISPATCH[Dispatch callbacks]
    HAS -->|No, timeout| TIMERS[Check timers]

    DISPATCH --> READ{POLLIN?}
    DISPATCH --> WRITE{POLLOUT?}
    DISPATCH --> ERR{POLLERR?}

    READ -->|Yes| CB_READ[on_read callback]
    WRITE -->|Yes| CB_WRITE[on_write callback]
    ERR -->|Yes| CB_ERR[on_error callback]

    CB_READ --> POLL
    CB_WRITE --> POLL
    CB_ERR --> POLL
    TIMERS --> POLL

    style INIT fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style POLL fill:#6b3a6b,stroke:#b06ab0,color:#fff
    style DISPATCH fill:#6b3a6b,stroke:#b06ab0,color:#fff
```

---

## DNS Resolution Flow

```mermaid
flowchart LR
    INPUT([hostname]) --> GAI[getaddrinfo]
    GAI --> ITER[Iterate results]

    ITER --> V4{AF_INET?}
    ITER --> V6{AF_INET6?}

    V4 -->|Yes| ADD4[Add to ipv4_addresses]
    V6 -->|Yes| ADD6[Add to ipv6_addresses]

    ADD4 --> RESULT
    ADD6 --> RESULT

    RESULT([DnsResult<br/>ipv4 + ipv6 + canonical])

    style INPUT fill:#2a6041,stroke:#4bc07a,color:#fff
    style RESULT fill:#2a6041,stroke:#4bc07a,color:#fff
```

---

## Design Patterns

### CRTP (Curiously Recurring Template Pattern)

```mermaid
graph TB
    BASE["IpBase&lt;Derived, T, N&gt;<br/>━━━━━━━━━━━━━━━<br/>m_address: array&lt;T, N&gt;<br/>bytes(): const ref<br/>operator&lt;=&gt;()"]

    IPV4["Ip&lt;4&gt; : IpBase&lt;Ip4, uint8_t, 4&gt;<br/>━━━━━━━━━━━━━━━<br/>parse(), to_uint32()<br/>operator+(), display()"]

    IPV6["Ip&lt;6&gt; : IpBase&lt;Ip6, uint16_t, 8&gt;<br/>━━━━━━━━━━━━━━━<br/>parse(), display()"]

    BASE --> IPV4
    BASE --> IPV6

    style BASE fill:#2d4a7a,stroke:#4a90d9,color:#fff
    style IPV4 fill:#2a6041,stroke:#4bc07a,color:#fff
    style IPV6 fill:#2a6041,stroke:#4bc07a,color:#fff
```

Zero-cost static polymorphism. The base class holds the address array and comparison operators; derived classes add version-specific parsing and arithmetic.

### RAII (Resource Acquisition Is Initialization)

All OS handles are managed with RAII:

| Class | Resource | Acquire | Release |
|-------|----------|---------|---------|
| `Socket` | socket fd | `::socket()` | `closesocket()` / `close()` |
| `WsaGuard` | WinSock | `WSAStartup()` | `WSACleanup()` |
| `CredentialGuard` | SSPI creds | `AcquireCredentialsHandle()` | `FreeCredentialsHandle()` |
| `ContextGuard` | Security ctx | `InitializeSecurityContext()` | `DeleteSecurityContext()` |

Move semantics supported; copy deleted on all.

### Template Specialization

```cpp
template <uint8_t Ipv> class Ip;     // Primary: static_assert guard
template <> class Ip<4> { ... };     // IPv4 specialization
template <> class Ip<6> { ... };     // IPv6 specialization
```

Invalid versions fail at compile-time via `static_assert`.

---

## Platform Abstraction

```mermaid
graph TB
    subgraph "Application Code"
        APP["Socket&lt;Ip&lt;4&gt;&gt;"]
    end

    subgraph "Platform Layer"
        direction LR
        WIN["Windows<br/>━━━━━━━━<br/>SOCKET type<br/>WSAStartup<br/>closesocket<br/>WSAGetLastError"]
        POSIX["POSIX<br/>━━━━━━━━<br/>int type<br/>no init needed<br/>close<br/>errno"]
    end

    APP --> WIN
    APP --> POSIX

    style APP fill:#2d4a7a,stroke:#4a90d9,color:#fff
    style WIN fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style POSIX fill:#2a6041,stroke:#4bc07a,color:#fff
```

| Abstraction | Windows | POSIX |
|-------------|---------|-------|
| `socket_t` | `SOCKET` | `int` |
| `invalid_socket` | `INVALID_SOCKET` | `-1` |
| `close_socket()` | `closesocket()` | `close()` |
| `last_error()` | `WSAGetLastError()` | `errno` |
| TLS | SChannel (SSPI) | *(stub)* |
| Ping | `IcmpSendEcho` | *(stub)* |
| Interfaces | `GetAdaptersAddresses` | *(stub)* |

---

## Key C++23 Features

| Feature | Usage |
|---------|-------|
| `std::print` | Formatted output in all `display()` methods |
| `constexpr` | Nearly all constructors and operators |
| `auto operator<=>` | Three-way comparison for all value types |
| `std::integral auto` | Abbreviated function templates with concepts |
| `std::byteswap` | Network byte order conversion |
| `std::span` | Buffer views in `send()` / `recv()` |
| `std::source_location` | Test framework failure reporting |
| CTAD | Deduction guides: `Ip(a,b,c,d)` → `Ip<4>` |

---

## WebSocket Frame Flow

```mermaid
flowchart LR
    subgraph Encode
        F["WsFrame"] --> B0["Byte 0: FIN + opcode"]
        F --> B1["Byte 1: MASK + length"]
        B1 --> LEN{length}
        LEN -->|"< 126"| S["1 byte"]
        LEN -->|"< 65536"| M["2 bytes ext"]
        LEN -->|"> 65535"| L["8 bytes ext"]
        S --> MASK{"masked?"}
        M --> MASK
        L --> MASK
        MASK -->|Yes| KEY["4-byte mask key"] --> PAY["XOR payload"]
        MASK -->|No| RAW["Raw payload"]
    end

    PAY --> OUT([Encoded bytes])
    RAW --> OUT

    style F fill:#7a5c2d,stroke:#d9a04a,color:#fff
    style OUT fill:#2a6041,stroke:#4bc07a,color:#fff
```

---

## Subnet Calculation

```mermaid
flowchart TD
    INPUT(["192.168.1.0/24"]) --> PARSE["Parse CIDR"]
    PARSE --> ADDR["Network: 192.168.1.0"]
    PARSE --> PREFIX["Prefix: /24"]

    PREFIX --> MASK["Mask: 255.255.255.0<br/>= ~((1 << 8) - 1)"]
    ADDR --> BCAST["Broadcast: 192.168.1.255<br/>= network | ~mask"]
    MASK --> HOSTS["Hosts: 254<br/>= (1 << 8) - 2"]

    ADDR --> CONTAINS{"contains(ip)?"}
    MASK --> CONTAINS
    CONTAINS -->|"ip & mask == network"| YES([true])
    CONTAINS -->|else| NO([false])

    style INPUT fill:#2a6041,stroke:#4bc07a,color:#fff
    style YES fill:#2a6041,stroke:#4bc07a,color:#fff
    style NO fill:#7a2d2d,stroke:#d94a4a,color:#fff
```

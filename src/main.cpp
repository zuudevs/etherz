#include "etherz.hpp"
#include "net/internet_protocol.hpp"
#include "net/socket_address.hpp"
#include "net/tcp.hpp"
#include "net/udp.hpp"
#include "net/socket.hpp"
#include "net/udp_socket.hpp"
#include "async/poll.hpp"
#include "async/event_loop.hpp"
#include "async/async_socket.hpp"
#include "protocol/url.hpp"
#include "protocol/http.hpp"
#include "protocol/http_client.hpp"
#include "protocol/http_server.hpp"
#include "protocol/websocket.hpp"
#include "core/error.hpp"
#include <print>
#include <windows.h>

namespace etn = etherz::net;
namespace etc = etherz::core;
namespace eta = etherz::async;
namespace etp = etherz::protocol;

/**
 * @brief Initialize console to properly handle UTF-8 output.
 *
 * This function configures the console so UTF-8 encoded text
 * (e.g. Unicode characters, emojis, symbols) can be displayed correctly.
 *
 * Platform behavior:
 * - Windows:
 *   - Sets input and output code page to UTF-8 (CP_UTF8).
 *   - Enables full buffering for stdout to reduce flickering
 *     and improve performance.
 *
 * - Linux / macOS:
 *   - No action is required.
 *   - Modern terminals already use UTF-8 by default.
 *
 * Safe to call multiple times.
 */
inline void utf8_console() {
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	setvbuf(stdout, nullptr, _IOFBF, 1024);

#elif defined(__linux__) || defined(__APPLE__)
	(void)0;
#else
	(void)0;
#endif
}

int main() {
	utf8_console();
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz v{} by {}\n", etherz::version(), etherz::author());
	std::print("═══════════════════════════════════\n\n");

	// ─── IPv4 ───────────────────────────
	std::print("── IPv4 ──────────────────────────\n");

	auto ip4a = etn::Ip(192, 168, 1, 50);
	auto ip4b = etn::Ip<4>{"10.0.0.1"};
	auto ip4c = etn::Ip<4>(0xC0A80101); // 192.168.1.1

	ip4a.display();
	ip4b.display();
	ip4c.display();

	// Arithmetic
	auto ip4d = ip4c + 5;
	std::print("192.168.1.1 + 5 = ");
	ip4d.display();
	++ip4d;
	std::print("After ++     = ");
	ip4d.display();

	// Network byte order
	std::print("Network order of 192.168.1.1: 0x{:08X}\n\n", ip4c.to_network());

	// ─── IPv6 ───────────────────────────
	std::print("── IPv6 ──────────────────────────\n");

	auto ip6a = etn::Ip<6>(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1);
	auto ip6b = etn::Ip<6>{"2001:0db8::1"};
	auto ip6c = etn::Ip<6>{"fe80::1"};
	auto ip6d = etn::Ip<6>{"2001:0db8:85a3:0000:0000:8a2e:0370:7334"};

	ip6a.display();
	ip6b.display();
	ip6c.display();
	ip6d.display();

	++ip6c;
	std::print("After ++ fe80::1 = ");
	ip6c.display();
	std::print("\n");

	// ─── SocketAddress ──────────────────
	std::print("── SocketAddress ─────────────────\n");

	auto sa4 = etn::SocketAddress<etn::Ip<4>>(etn::Ip(127, 0, 0, 1), 8080);
	auto sa4b = etn::SocketAddress<etn::Ip<4>>(etn::Ip<4>{"192.168.1.100"}, 443);
	auto sa4c = etn::SocketAddress<etn::Ip<4>>("0.0.0.0", "3000");

	sa4.display();
	sa4b.display();
	sa4c.display();

	auto sa6 = etn::SocketAddress<etn::Ip<6>>(
		etn::Ip<6>(0, 0, 0, 0, 0, 0, 0, 1), 8080
	);
	sa6.display();
	std::print("\n");

	// ─── TCP Endpoints ──────────────────
	std::print("── TCP Endpoints ─────────────────\n");

	auto tcp4 = etn::Tcp<etn::Ip<4>>(etn::Ip(10, 0, 0, 1), 80);
	auto tcp6 = etn::Tcp<etn::Ip<6>>(etn::Ip<6>{"::1"}, 443);

	tcp4.display();
	tcp6.display();
	std::print("\n");

	// ─── UDP Endpoints (v0.2.0) ────────
	std::print("── UDP Endpoints ─────────────────\n");

	auto udp4 = etn::Udp<etn::Ip<4>>(etn::Ip(8, 8, 8, 8), 53);
	auto udp6 = etn::Udp<etn::Ip<6>>(etn::Ip<6>{"::1"}, 5353);

	udp4.display();
	udp6.display();
	std::print("\n");

	// ─── Socket Options (v0.2.0) ───────
	std::print("── Socket Options (v0.2.0) ───────\n");
	{
		etn::Socket<etn::Ip<4>> sock;
		auto err = sock.create();
		std::print("create : {}\n", etc::error_message(err));

		err = sock.set_reuse_addr(true);
		std::print("reuse  : {}\n", etc::error_message(err));

		err = sock.set_nonblocking(true);
		std::print("nonblk : {}\n", etc::error_message(err));

		err = sock.set_timeout(5000);
		std::print("timeout: {}\n", etc::error_message(err));

		err = sock.shutdown(etc::ShutdownMode::Both);
		std::print("shutdn : {} (expected: not connected)\n", etc::error_message(err));
		// sock auto-closes here (RAII)
	}
	std::print("\n");

	// ─── IPv6 Socket (v0.2.0) ──────────
	std::print("── IPv6 Socket (v0.2.0) ──────────\n");
	{
		etn::Socket<etn::Ip<6>> sock6;
		auto err = sock6.create();
		std::print("IPv6 socket create: {}\n", etc::error_message(err));
		std::print("IPv6 socket open  : {}\n", sock6.is_open());
	}
	std::print("\n");

	// ─── UdpSocket (v0.2.0) ────────────
	std::print("── UdpSocket (v0.2.0) ────────────\n");
	{
		etn::UdpSocket<etn::Ip<4>> udp_sock;
		auto err = udp_sock.create();
		std::print("UDP create : {}\n", etc::error_message(err));

		err = udp_sock.set_reuse_addr(true);
		std::print("UDP reuse  : {}\n", etc::error_message(err));

		std::print("UDP open   : {}\n", udp_sock.is_open());
	}
	std::print("\n");

	// ─── Error Mapping (v0.2.0) ────────
	std::print("── Error Types & Mapping ─────────\n");

	auto err1 = etc::Error::None;
	auto err2 = etc::Error::ConnectionRefused;
	auto err3 = etc::Error::WouldBlock;

	std::print("{}: {} (ok={})\n", 
		static_cast<int>(err1), etc::error_message(err1), etc::is_ok(err1));
	std::print("{}: {} (ok={})\n", 
		static_cast<int>(err2), etc::error_message(err2), etc::is_ok(err2));
	std::print("{}: {} (ok={})\n",
		static_cast<int>(err3), etc::error_message(err3), etc::is_ok(err3));
	std::print("\n");

	// ─── Poll (v0.3.0) ─────────────────
	std::print("── Poll I/O (v0.3.0) ─────────────\n");
	{
		etn::Socket<etn::Ip<4>> poll_sock;
		poll_sock.create();
		poll_sock.set_nonblocking(true);

		eta::PollEntry entries[1];
		entries[0].fd = poll_sock.native_handle();
		entries[0].requested = eta::PollEvent::WriteReady;

		int ready = eta::poll(entries, 0); // non-blocking poll
		std::print("Polled 1 socket (0ms): {} ready\n", ready);
		if (ready > 0) {
			std::print("  WriteReady: {}\n",
				eta::has_event(entries[0].returned, eta::PollEvent::WriteReady));
		}
	}
	std::print("\n");

	// ─── EventLoop (v0.3.0) ────────────
	std::print("── EventLoop (v0.3.0) ────────────\n");
	{
		eta::EventLoop loop;

		etn::Socket<etn::Ip<4>> ev_sock;
		ev_sock.create();
		ev_sock.set_nonblocking(true);

		bool callback_fired = false;
		loop.add(ev_sock.native_handle(), eta::PollEvent::WriteReady,
			[&callback_fired, &loop](etn::impl::socket_t fd, eta::PollEvent /*ev*/) {
				callback_fired = true;
				loop.remove(fd);
			});

		std::print("Registered fds : {}\n", loop.size());
		loop.run_once(0);
		std::print("Callback fired : {}\n", callback_fired);
		std::print("Remaining fds  : {}\n", loop.size());
	}
	std::print("\n");

	// ─── AsyncSocket (v0.3.0) ──────────
	std::print("── AsyncSocket (v0.3.0) ──────────\n");
	{
		eta::AsyncSocket<etn::Ip<4>> async_sock;
		auto err = async_sock.create();
		std::print("Async create   : {}\n", etc::error_message(err));
		std::print("Async is_open  : {}\n", async_sock.is_open());

		err = async_sock.set_reuse_addr(true);
		std::print("Async reuse    : {}\n", etc::error_message(err));
	}
	std::print("\n");

	// ─── URL Parsing (v0.4.0) ──────────
	std::print("── URL Parsing (v0.4.0) ──────────\n");
	{
		auto url = etp::Url::parse("http://example.com:8080/api/v1?key=val#section");
		url.display();

		auto url2 = etp::Url::parse("https://localhost/index.html");
		url2.display();
		std::print("Reconstructed: {}\n", url2.to_string());
	}
	std::print("\n");

	// ─── HTTP Core (v0.4.0) ────────────
	std::print("── HTTP Core (v0.4.0) ────────────\n");
	{
		// Build & serialize a request
		etp::HttpRequest req;
		req.method = etp::HttpMethod::Post;
		req.path = "/api/data";
		req.headers.set("Host", "example.com");
		req.headers.set("Content-Type", "application/json");
		req.body = R"({"key":"value"})";
		req.display();
		std::print("Serialized ({} bytes)\n", req.serialize().size());

		// Parse a raw response
		std::string_view raw_resp =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"\r\n"
			"<h1>Hello</h1>";
		auto resp = etp::http_parser::parse_response(raw_resp);
		resp.display();
		std::print("Body: {}\n", resp.body);
	}
	std::print("\n");

	// ─── HttpServer routes (v0.4.0) ─────
	std::print("── HttpServer (v0.4.0) ───────────\n");
	{
		etp::HttpServer server;
		server.get("/", [](const etp::HttpRequest&) {
			etp::HttpResponse r;
			r.status = etp::HttpStatus::OK;
			r.body = "Hello, World!";
			return r;
		});
		server.post("/echo", [](const etp::HttpRequest& req) {
			etp::HttpResponse r;
			r.status = etp::HttpStatus::OK;
			r.body = req.body;
			return r;
		});
		std::print("Routes registered: {}\n", server.route_count());
	}
	std::print("\n");

	// ─── WebSocket Frames (v0.4.0) ──────
	std::print("── WebSocket (v0.4.0) ────────────\n");
	{
		// Encode a text frame
		etp::WsFrame frame;
		frame.set_text("Hello WS!");
		frame.display();

		auto encoded = etp::ws_encode_frame(frame);
		std::print("Encoded: {} bytes\n", encoded.size());

		// Decode it back
		auto decoded = etp::ws_decode_frame(encoded);
		decoded.display();
		std::print("Payload: {}\n", decoded.payload_text());
	}
	std::print("\n");

	// ─── Comparison ─────────────────────
	std::print("── Comparison ────────────────────\n");

	auto cmp1 = etn::Ip(192, 168, 1, 1);
	auto cmp2 = etn::Ip(192, 168, 1, 2);
	std::print("192.168.1.1 == 192.168.1.2 ? {}\n", cmp1 == cmp2);
	std::print("192.168.1.1 <  192.168.1.2 ? {}\n", cmp1 < cmp2);
	std::print("192.168.1.1 == 192.168.1.1 ? {}\n", cmp1 == cmp1);

	std::print("\n═══════════════════════════════════\n");
	std::print("  All demos completed successfully!\n");
	std::print("═══════════════════════════════════\n");

	return 0;
}
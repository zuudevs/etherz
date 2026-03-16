/**
 * @file dtls_chat.cpp
 * @brief Secure UDP chat using DTLS
 * 
 * Usage:
 *   dtls_chat server     — Start DTLS server
 *   dtls_chat client     — Connect as client
 */

#include "../include/security/dtls_socket.hpp"
#include "../include/security/dtls_context.hpp"
#include "../include/net/internet_protocol.hpp"
#include <print>
#include <string>
#include <array>

namespace es = etherz::security;
namespace en = etherz::net;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: dtls_chat <server|client>\n");
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "server") {
		auto config = es::DtlsConfig::server("server.pem", "server-key.pem");
		es::DtlsContext ctx(config);
		auto err = ctx.initialize();
		if (ec::is_error(err)) {
			std::print("Context init failed\n");
			return 1;
		}

		es::DtlsSocket<en::Ip<4>> sock(ctx);
		sock.create();
		sock.bind(en::SocketAddress<en::Ip<4>>(en::Ip<4>(0, 0, 0, 0), 4433));
		std::print("DTLS server on port 4433\n");

		// Wait for client and perform handshake
		std::array<uint8_t, 1400> buffer{};
		auto result = sock.recv(buffer);
		if (result.bytes > 0) {
			sock.set_peer(result.sender);
			sock.handshake();

			std::string msg(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(result.bytes));
			std::print("Received: {}\n", msg);

			// Echo back
			sock.send(std::span<const uint8_t>(buffer.data(),
				static_cast<size_t>(result.bytes)));
		}
	}
	else if (mode == "client") {
		auto config = es::DtlsConfig::client();
		config.verify_peer = false;  // Skip for demo
		es::DtlsContext ctx(config);
		ctx.initialize();

		es::DtlsSocket<en::Ip<4>> sock(ctx);
		sock.create();
		sock.set_peer(en::SocketAddress<en::Ip<4>>(en::Ip<4>(127, 0, 0, 1), 4433));
		sock.handshake();

		std::string message = "Hello, DTLS!";
		sock.send(std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(message.data()), message.size()));

		std::array<uint8_t, 1400> buffer{};
		auto result = sock.recv(buffer);
		if (result.bytes > 0) {
			std::string reply(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(result.bytes));
			std::print("Reply: {}\n", reply);
		}
	}

	return 0;
}

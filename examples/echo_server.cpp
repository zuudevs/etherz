/**
 * @file echo_server.cpp
 * @brief Simple TCP echo server example
 * 
 * Demonstrates Socket<Ip<4>> usage with a basic TCP echo server.
 * Usage: echo_server [port]
 */

#include "etherz.hpp"
#include "net/internet_protocol.hpp"
#include "net/socket_address.hpp"
#include "net/tcp.hpp"
#include "net/socket.hpp"
#include "core/error.hpp"
#include <span>
#include <print>

#ifdef _WIN32
	#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	uint16_t port = 8080;
	if (argc > 1) {
		port = static_cast<uint16_t>(std::atoi(argv[1]));
	}

	std::print("╔═══════════════════════════════╗\n");
	std::print("║  Etherz Echo Server v1.0.0    ║\n");
	std::print("╚═══════════════════════════════╝\n\n");

	// Create server socket
	etherz::net::Socket<etherz::net::Ip<4>> server;
	auto err = server.create();
	if (err != etherz::core::Error::None) {
		std::print("Failed to create socket: {}\n", etherz::core::error_message(err));
		return 1;
	}

	// Set reuse address
	server.set_reuse_addr(true);

	// Bind
	auto addr = etherz::net::SocketAddress<etherz::net::Ip<4>>(
		etherz::net::Ip<4>(0, 0, 0, 0), port);
	err = server.bind(addr);
	if (err != etherz::core::Error::None) {
		std::print("Failed to bind: {}\n", etherz::core::error_message(err));
		return 1;
	}

	// Listen
	err = server.listen(5);
	if (err != etherz::core::Error::None) {
		std::print("Failed to listen: {}\n", etherz::core::error_message(err));
		return 1;
	}

	std::print("Listening on 0.0.0.0:{}\n", port);
	std::print("Press Ctrl+C to stop\n\n");

	// Accept loop
	while (true) {
		auto accept_result = server.accept();
		if (!accept_result) {
			std::print("Accept failed: {}\n", accept_result.error());
			continue;
		}

		auto client = std::move(accept_result->socket);
		std::print("Client connected!\n");

		// Echo loop
		uint8_t buf[1024]{};
		while (true) {
			int n = client.recv(std::span<uint8_t>(buf, sizeof(buf) - 1));
			if (n <= 0) break;

			buf[n] = '\0';
			std::print("Received: {}\n", reinterpret_cast<const char*>(buf));

			int sent = client.send(std::span<const uint8_t>(buf, static_cast<size_t>(n)));
			if (sent <= 0) break;
		}

		std::print("Client disconnected\n");
	}

	return 0;
}

/**
 * @file unix_echo.cpp
 * @brief Unix domain socket echo server/client example
 * 
 * Usage:
 *   unix_echo server     — Start echo server
 *   unix_echo client     — Connect and send a message
 */

#include "../include/net/unix_socket.hpp"
#include <print>
#include <string>
#include <array>

namespace en = etherz::net;
namespace ec = etherz::core;

#ifdef _WIN32
constexpr auto SOCKET_PATH = "etherz_echo.sock";
#else
constexpr auto SOCKET_PATH = "/tmp/etherz_echo.sock";
#endif

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: unix_echo <server|client>\n");
		return 1;
	}

	std::string mode = argv[1];
	auto addr = en::UnixSocketAddress(SOCKET_PATH);

	if (mode == "server") {
		en::UnixSocket server;
		if (auto err = server.create(); ec::is_error(err)) {
			std::print("Create error: {}\n", ec::error_message(err));
			return 1;
		}
		if (auto err = server.bind(addr); ec::is_error(err)) {
			std::print("Bind error: {}\n", ec::error_message(err));
			return 1;
		}
		if (auto err = server.listen(); ec::is_error(err)) {
			std::print("Listen error: {}\n", ec::error_message(err));
			return 1;
		}

		std::print("Echo server listening on {}\n", SOCKET_PATH);

		while (true) {
			auto result = server.accept();
			if (!result) {
				std::print("Accept error: {}\n", ec::error_message(result.error()));
				continue;
			}

			auto client = std::move(*result);
			std::array<uint8_t, 1024> buffer{};
			int received = client.recv(buffer);
			if (received > 0) {
				std::string msg(reinterpret_cast<const char*>(buffer.data()),
					static_cast<size_t>(received));
				std::print("Received: {}\n", msg);
				client.send(std::span<const uint8_t>(buffer.data(),
					static_cast<size_t>(received)));
			}
			client.close();
		}
	}
	else if (mode == "client") {
		en::UnixSocket client;
		if (auto err = client.create(); ec::is_error(err)) {
			std::print("Create error: {}\n", ec::error_message(err));
			return 1;
		}
		if (auto err = client.connect(addr); ec::is_error(err)) {
			std::print("Connect error: {}\n", ec::error_message(err));
			return 1;
		}

		std::string msg = "Hello from Etherz Unix socket!";
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
		client.send(data);

		std::array<uint8_t, 1024> buffer{};
		int received = client.recv(buffer);
		if (received > 0) {
			std::string echo(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));
			std::print("Echo: {}\n", echo);
		}
	}

	return 0;
}

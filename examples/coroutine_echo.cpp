/**
 * @file coroutine_echo.cpp
 * @brief Coroutine-based echo server using Task<T> and Generator<T>
 * 
 * Demonstrates C++20 coroutine integration with Etherz networking.
 */

#include "../include/async/task.hpp"
#include "../include/async/generator.hpp"
#include "../include/async/timer.hpp"
#include "../include/net/socket.hpp"
#include "../include/net/socket_address.hpp"
#include "../include/net/internet_protocol.hpp"
#include <print>
#include <array>
#include <string>

namespace en = etherz::net;
namespace ea = etherz::async;
namespace ec = etherz::core;

// Generator that yields received messages from a socket
ea::Generator<std::string> receive_messages(en::Socket<en::Ip<4>>& client) {
	std::array<uint8_t, 1024> buffer{};
	while (true) {
		int received = client.recv(buffer);
		if (received <= 0) break;
		co_yield std::string(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));
	}
}

// Coroutine task that handles a single client
ea::Task<int> handle_client(en::Socket<en::Ip<4>> client) {
	std::print("Client connected\n");
	int total = 0;

	std::array<uint8_t, 1024> buffer{};
	while (true) {
		int received = client.recv(buffer);
		if (received <= 0) break;

		// Echo back
		client.send(std::span<const uint8_t>(buffer.data(),
			static_cast<size_t>(received)));
		total += received;

		std::string msg(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));
		std::print("Echoed: {}\n", msg);
	}

	std::print("Client disconnected (total {} bytes)\n", total);
	co_return total;
}

// Task that demonstrates timer usage
ea::Task<void> delayed_greeting() {
	std::print("Starting delayed greeting...\n");
	co_await ea::Timer::delay(std::chrono::seconds(1));
	std::print("Hello after 1 second!\n");
}

int main() {
	constexpr uint16_t PORT = 8090;

	en::Socket<en::Ip<4>> server;
	if (auto err = server.create(); ec::is_error(err)) {
		std::print("Create failed: {}\n", ec::error_message(err));
		return 1;
	}

	server.set_reuse_addr(true);

	auto addr = en::SocketAddress<en::Ip<4>>(en::Ip<4>(0, 0, 0, 0), PORT);
	if (auto err = server.bind(addr); ec::is_error(err)) {
		std::print("Bind failed: {}\n", ec::error_message(err));
		return 1;
	}

	if (auto err = server.listen(); ec::is_error(err)) {
		std::print("Listen failed: {}\n", ec::error_message(err));
		return 1;
	}

	std::print("Coroutine echo server on port {}\n", PORT);

	// Demonstrate generator: number range
	std::print("Generator demo: ");
	auto nums = [](int n) -> ea::Generator<int> {
		for (int i = 1; i <= n; ++i)
			co_yield i * i;
	};
	for (int sq : nums(5)) {
		std::print("{} ", sq);
	}
	std::print("\n");

	// Main server loop
	while (true) {
		auto result = server.accept();
		if (!result) continue;

		auto task = handle_client(std::move(result->socket));
		int bytes = task.run();
		std::print("Session: {} bytes transferred\n", bytes);
	}

	return 0;
}

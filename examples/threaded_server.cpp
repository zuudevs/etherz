/**
 * @file threaded_server.cpp
 * @brief Multi-threaded HTTP server using ThreadPool + ParallelAcceptor
 * 
 * Demonstrates:
 * - ParallelAcceptor dispatching connections to a thread pool
 * - Serving HTTP responses from worker threads
 * - Graceful shutdown
 */

#include "async/parallel_socket.hpp"
#include "protocol/http.hpp"
#include <print>
#include <thread>
#include <chrono>
#include <span>

namespace etn = etherz::net;
namespace eta = etherz::async;
namespace etp = etherz::protocol;

int main() {
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz Threaded Server Example\n");
	std::print("═══════════════════════════════════\n\n");

	eta::ParallelAcceptor<etn::Ip<4>> server;

	auto handler = [](etn::Socket<etn::Ip<4>>&& client) {
		// Read HTTP request
		std::array<uint8_t, 4096> buffer{};
		int received = client.recv(buffer);
		if (received <= 0) {
			client.close();
			return;
		}

		std::string_view request(
			reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		// Get thread ID for response
		auto tid = std::this_thread::get_id();
		std::ostringstream oss;
		oss << tid;

		// Build response
		std::string body = "{\"message\":\"Hello from Etherz!\","
			"\"thread\":\"" + oss.str() + "\"}";

		etp::HttpResponse resp;
		resp.status = etp::HttpStatus::OK;
		resp.headers.set("Content-Type", "application/json");
		resp.headers.set("Content-Length", std::to_string(body.size()));
		resp.headers.set("Connection", "close");
		resp.headers.set("Server", "Etherz/4.0.0");
		resp.body = body;

		auto raw = resp.serialize();
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		client.send(data);
		client.close();
	};

	eta::ParallelConfig config;
	config.pool_threads    = 4;
	config.max_connections = 100;

	auto err = server.start(8080, handler, config);
	if (etherz::core::is_error(err)) {
		std::print("Failed to start: {}\n",
			etherz::core::error_message(err));
		return 1;
	}

	std::print("Server running on http://127.0.0.1:8080\n");
	std::print("Thread pool: {} workers\n", 4);
	std::print("Press Ctrl+C to stop (auto-stops in 60s)\n\n");

	// Run for 60 seconds
	auto start = std::chrono::steady_clock::now();
	while (server.is_running()) {
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::print("  Active: {}  Total accepted: {}\n",
			server.active_connections(), server.total_accepted());

		auto elapsed = std::chrono::steady_clock::now() - start;
		if (elapsed > std::chrono::seconds(60)) break;
	}

	server.stop();
	std::print("\nServer stopped. Total connections: {}\n",
		server.total_accepted());
	return 0;
}

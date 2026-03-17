/**
 * @file ws_chat.cpp
 * @brief WebSocket chat server + client example
 * 
 * Demonstrates:
 * - WsServer with connect/message/disconnect callbacks
 * - WsClient connecting and sending messages
 * - Broadcasting messages to all connected clients
 */

#include "protocol/ws_server.hpp"
#include "protocol/ws_client.hpp"
#include <print>
#include <thread>
#include <chrono>

namespace etp = etherz::protocol;

void run_server() {
	etp::WsServer server;

	server.on_connect([](etp::WsConnection& conn) {
		std::print("[Server] Client {} connected\n", conn.id());
		conn.send_text("Welcome to Etherz WebSocket Chat!");
	});

	server.on_message([&server](etp::WsConnection& conn, const etp::WsMessage& msg) {
		std::print("[Server] Client {}: {}\n", conn.id(), msg.text());
		// Broadcast to all
		server.broadcast("[" + std::to_string(conn.id()) + "]: " + msg.text());
	});

	server.on_disconnect([](uint32_t id) {
		std::print("[Server] Client {} disconnected\n", id);
	});

	auto err = server.listen(9090);
	if (etherz::core::is_error(err)) {
		std::print("[Server] Failed to start: {}\n",
			etherz::core::error_message(err));
		return;
	}

	std::print("[Server] Listening on ws://127.0.0.1:9090\n");

	// Accept and process for ~30 seconds
	auto start = std::chrono::steady_clock::now();
	while (server.is_running()) {
		server.poll();

		auto elapsed = std::chrono::steady_clock::now() - start;
		if (elapsed > std::chrono::seconds(30)) break;
	}

	server.stop();
	std::print("[Server] Stopped\n");
}

void run_client() {
	// Give server time to start
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	etp::WsClient client;
	auto err = client.connect("ws://127.0.0.1:9090");
	if (etherz::core::is_error(err)) {
		std::print("[Client] Connection failed: {}\n",
			etherz::core::error_message(err));
		return;
	}

	std::print("[Client] Connected to server\n");

	// Receive welcome message
	auto welcome = client.recv();
	if (welcome.has_value()) {
		std::print("[Client] Server says: {}\n", welcome->text());
	}

	// Send some messages
	const char* messages[] = {"Hello!", "How are you?", "Goodbye!"};
	for (auto msg : messages) {
		client.send_text(msg);
		std::print("[Client] Sent: {}\n", msg);
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		auto reply = client.recv();
		if (reply.has_value()) {
			std::print("[Client] Received: {}\n", reply->text());
		}
	}

	client.close();
	std::print("[Client] Disconnected\n");
}

int main() {
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz WebSocket Chat Example\n");
	std::print("═══════════════════════════════════\n\n");

	// Run server and client in separate threads
	std::thread server_thread(run_server);
	std::thread client_thread(run_client);

	client_thread.join();

	// Give a moment for cleanup
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// Server will stop after timeout
	server_thread.join();

	return 0;
}

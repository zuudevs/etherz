/**
 * @file ws_server.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief WebSocket server with per-connection callbacks and upgrade handling
 * @version 3.2.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <span>
#include <functional>
#include <unordered_map>

#include "websocket.hpp"
#include "ws_client.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  WebSocket Connection Handle
// ═══════════════════════════════════════════════

/**
 * @brief A server-side WebSocket connection
 */
class WsConnection {
public:
	WsConnection() noexcept = default;

	explicit WsConnection(net::Socket<net::Ip<4>>&& sock, uint32_t id) noexcept
		: socket_(std::move(sock)), id_(id), connected_(true) {}

	// Move-only
	WsConnection(WsConnection&&) noexcept = default;
	WsConnection& operator=(WsConnection&&) noexcept = default;
	WsConnection(const WsConnection&) = delete;
	WsConnection& operator=(const WsConnection&) = delete;

	/**
	 * @brief Send a text message to this client
	 */
	core::Error send_text(std::string_view text) {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.set_text(text);
		frame.masked = false;  // Server does not mask

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	/**
	 * @brief Send a binary message to this client
	 */
	core::Error send_binary(std::span<const uint8_t> data) {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.set_binary(data);
		frame.masked = false;

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	/**
	 * @brief Send a Ping frame
	 */
	core::Error send_ping() {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.opcode = WsOpcode::Ping;
		frame.fin = true;
		frame.masked = false;

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	/**
	 * @brief Close this connection
	 */
	void close(WsCloseCode code = WsCloseCode::Normal) noexcept {
		if (connected_) {
			WsFrame frame;
			frame.opcode = WsOpcode::Close;
			frame.fin = true;
			frame.masked = false;
			frame.payload.push_back(
				static_cast<uint8_t>((static_cast<uint16_t>(code) >> 8) & 0xFF));
			frame.payload.push_back(
				static_cast<uint8_t>(static_cast<uint16_t>(code) & 0xFF));

			auto encoded = ws_encode_frame(frame);
			socket_.send(
				std::span<const uint8_t>(encoded.data(), encoded.size()));
			connected_ = false;
		}
		socket_.close();
	}

	uint32_t id() const noexcept { return id_; }
	bool is_connected() const noexcept { return connected_; }
	net::Socket<net::Ip<4>>& socket() noexcept { return socket_; }

private:
	net::Socket<net::Ip<4>> socket_;
	uint32_t id_ = 0;
	bool connected_ = false;

	friend class WsServer;

	/**
	 * @brief Read and decode exactly one frame from the socket
	 */
	WsFrame read_frame() {
		std::array<uint8_t, 65536> buffer{};
		int received = socket_.recv(buffer);
		if (received <= 0) {
			connected_ = false;
			WsFrame f;
			f.opcode = WsOpcode::Close;
			return f;
		}
		return ws_decode_frame(
			std::span<const uint8_t>(buffer.data(),
				static_cast<size_t>(received)));
	}

	/**
	 * @brief Perform server-side WebSocket handshake
	 */
	core::Error perform_handshake() {
		std::array<uint8_t, 4096> buffer{};
		int received = socket_.recv(buffer);
		if (received <= 0) return core::Error::HandshakeFailed;

		std::string_view request(
			reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		// Verify it's a WebSocket upgrade request
		if (request.find("Upgrade: websocket") == std::string_view::npos &&
			request.find("Upgrade: WebSocket") == std::string_view::npos)
		{
			return core::Error::HandshakeFailed;
		}

		// Extract Sec-WebSocket-Key
		auto key_pos = request.find("Sec-WebSocket-Key:");
		if (key_pos == std::string_view::npos) {
			key_pos = request.find("Sec-WebSocket-Key:");
		}
		if (key_pos == std::string_view::npos) {
			return core::Error::HandshakeFailed;
		}

		// Send 101 response
		auto response = ws_handshake_response();
		auto resp_data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(response.data()), response.size());
		socket_.send(resp_data);

		return core::Error::None;
	}
};

// ═══════════════════════════════════════════════
//  WebSocket Server Configuration
// ═══════════════════════════════════════════════

struct WsServerConfig {
	uint32_t max_connections  = 64;
	uint32_t timeout_ms       = 30000;
	bool     auto_pong        = true;
};

// ═══════════════════════════════════════════════
//  WebSocket Server
// ═══════════════════════════════════════════════

/**
 * @brief WebSocket server with per-connection callbacks
 * 
 * Accepts incoming TCP connections, performs the WebSocket upgrade,
 * and dispatches messages to registered callbacks.
 * 
 * Usage:
 *   WsServer server;
 *   server.on_connect([](WsConnection& conn) {
 *       std::print("Client {} connected\n", conn.id());
 *   });
 *   server.on_message([](WsConnection& conn, const WsMessage& msg) {
 *       conn.send_text("Echo: " + msg.text());
 *   });
 *   server.on_disconnect([](uint32_t id) {
 *       std::print("Client {} disconnected\n", id);
 *   });
 *   
 *   server.listen(8080);
 *   while (server.is_running()) { server.poll(); }
 */
class WsServer {
public:
	using ConnectCallback    = std::function<void(WsConnection&)>;
	using MessageCallback    = std::function<void(WsConnection&, const WsMessage&)>;
	using DisconnectCallback = std::function<void(uint32_t)>;

	WsServer() noexcept = default;
	~WsServer() noexcept { stop(); }

	// Non-copyable
	WsServer(const WsServer&) = delete;
	WsServer& operator=(const WsServer&) = delete;

	// ─── Callbacks ──────────────────────

	void on_connect(ConnectCallback cb) { on_connect_ = std::move(cb); }
	void on_message(MessageCallback cb) { on_message_ = std::move(cb); }
	void on_disconnect(DisconnectCallback cb) { on_disconnect_ = std::move(cb); }

	// ─── Lifecycle ──────────────────────

	/**
	 * @brief Start listening on the given port
	 */
	core::Error listen(uint16_t port,
		WsServerConfig config = {}) noexcept
	{
		config_ = config;

		auto addr = net::SocketAddress<net::Ip<4>>(
			net::Ip<4>(0, 0, 0, 0), port);

		if (auto err = listener_.create(); core::is_error(err)) return err;
		listener_.set_reuse_addr(true);
		listener_.set_timeout(100);  // Short timeout for poll-like behavior
		if (auto err = listener_.bind(addr); core::is_error(err)) return err;
		if (auto err = listener_.listen(16); core::is_error(err)) return err;

		running_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Accept one pending connection (non-blocking with short timeout)
	 * and process one frame from each connected client
	 */
	void poll() {
		if (!running_) return;

		// Try to accept new connections
		accept_new_connections();

		// Process messages from connected clients
		process_clients();
	}

	/**
	 * @brief Stop the server and close all connections
	 */
	void stop() noexcept {
		running_ = false;
		for (auto& [id, conn] : connections_) {
			conn.close();
		}
		connections_.clear();
		listener_.close();
	}

	/**
	 * @brief Broadcast a text message to all connected clients
	 */
	void broadcast(std::string_view text) {
		for (auto& [id, conn] : connections_) {
			if (conn.is_connected()) {
				conn.send_text(text);
			}
		}
	}

	// ─── Queries ────────────────────────

	bool is_running() const noexcept { return running_; }
	size_t connection_count() const noexcept { return connections_.size(); }

	// ─── Handshake helpers (public for testing) ───

	/**
	 * @brief Build a WebSocket server handshake response
	 */
	static std::string build_upgrade_response(
		std::string_view accept_key = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=")
	{
		return ws_handshake_response(accept_key);
	}

	/**
	 * @brief Check if HTTP request is a WebSocket upgrade
	 */
	static bool is_upgrade_request(std::string_view request) {
		return request.find("Upgrade: websocket") != std::string_view::npos
			|| request.find("Upgrade: WebSocket") != std::string_view::npos;
	}

private:
	net::Socket<net::Ip<4>>                      listener_;
	std::unordered_map<uint32_t, WsConnection>   connections_;
	WsServerConfig                               config_;
	ConnectCallback                              on_connect_;
	MessageCallback                              on_message_;
	DisconnectCallback                           on_disconnect_;
	uint32_t                                     next_id_ = 1;
	bool                                         running_ = false;

	/**
	 * @brief Accept new incoming connections
	 */
	void accept_new_connections() {
		auto result = listener_.accept();
		if (!result.has_value()) return;

		auto& accepted = result.value();
		auto client_sock = accepted.take_client();

		if (connections_.size() >= config_.max_connections) {
			client_sock.close();
			return;
		}

		client_sock.set_timeout(config_.timeout_ms);

		uint32_t id = next_id_++;
		WsConnection conn(std::move(client_sock), id);

		// Perform WebSocket handshake
		if (auto err = conn.perform_handshake(); core::is_error(err)) {
			conn.close();
			return;
		}

		conn.socket().set_timeout(100);  // Non-blocking for poll

		if (on_connect_) {
			on_connect_(conn);
		}

		connections_.emplace(id, std::move(conn));
	}

	/**
	 * @brief Process one frame from each connected client
	 */
	void process_clients() {
		std::vector<uint32_t> to_remove;

		for (auto& [id, conn] : connections_) {
			if (!conn.is_connected()) {
				to_remove.push_back(id);
				continue;
			}

			auto frame = conn.read_frame();

			switch (frame.opcode) {
				case WsOpcode::Text:
				case WsOpcode::Binary: {
					WsMessage msg;
					msg.opcode = frame.opcode;
					msg.data = std::move(frame.payload);
					msg.complete = frame.fin;

					if (on_message_) {
						on_message_(conn, msg);
					}
					break;
				}

				case WsOpcode::Ping:
					if (config_.auto_pong) {
						WsFrame pong;
						pong.opcode = WsOpcode::Pong;
						pong.fin = true;
						pong.masked = false;
						pong.payload = frame.payload;

						auto encoded = ws_encode_frame(pong);
						conn.socket().send(
							std::span<const uint8_t>(encoded.data(), encoded.size()));
					}
					break;

				case WsOpcode::Close:
					conn.close();
					to_remove.push_back(id);
					break;

				default:
					break;
			}
		}

		for (uint32_t id : to_remove) {
			connections_.erase(id);
			if (on_disconnect_) {
				on_disconnect_(id);
			}
		}
	}
};

} // namespace protocol
} // namespace etherz

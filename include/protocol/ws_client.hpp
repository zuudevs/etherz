/**
 * @file ws_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief High-level WebSocket client with auto ping/pong and close handshake
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
#include <expected>
#include <functional>
#include <random>

#include "websocket.hpp"
#include "url.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  WebSocket Client Options
// ═══════════════════════════════════════════════

/**
 * @brief Configuration for a WebSocket connection
 */
struct WsClientConfig {
	uint32_t    timeout_ms       = 10000;   // Connection/recv timeout
	bool        auto_pong        = true;    // Automatically reply to Ping
	bool        auto_reconnect   = false;   // Auto-reconnect on disconnect
	std::string origin;                     // Origin header (optional)
	std::string protocol;                   // Sec-WebSocket-Protocol (optional)
	std::vector<std::pair<std::string, std::string>> extra_headers;
};

// ═══════════════════════════════════════════════
//  WebSocket Close Code
// ═══════════════════════════════════════════════

enum class WsCloseCode : uint16_t {
	Normal          = 1000,
	GoingAway       = 1001,
	ProtocolError   = 1002,
	UnsupportedData = 1003,
	NoStatus        = 1005,
	Abnormal        = 1006,
	InvalidPayload  = 1007,
	PolicyViolation = 1008,
	MessageTooBig   = 1009,
	MissingExtension = 1010,
	InternalError   = 1011,
};

// ═══════════════════════════════════════════════
//  WebSocket Message
// ═══════════════════════════════════════════════

/**
 * @brief A complete WebSocket message (may span multiple frames)
 */
struct WsMessage {
	WsOpcode             opcode = WsOpcode::Text;
	std::vector<uint8_t> data;
	bool                 complete = true;

	std::string text() const {
		return std::string(data.begin(), data.end());
	}

	std::span<const uint8_t> binary() const {
		return std::span<const uint8_t>(data.data(), data.size());
	}
};

// ═══════════════════════════════════════════════
//  WebSocket Client
// ═══════════════════════════════════════════════

/**
 * @brief High-level WebSocket client
 * 
 * Connects to a WebSocket server, performs the HTTP upgrade handshake,
 * and provides send/recv for text and binary messages. Automatically
 * handles Ping/Pong control frames and the Close handshake.
 * 
 * Usage:
 *   WsClient ws;
 *   auto err = ws.connect("ws://echo.example.com/ws");
 *   ws.send_text("Hello!");
 *   
 *   auto msg = ws.recv();
 *   if (msg) std::print("Got: {}\n", msg->text());
 *   
 *   ws.close();
 */
class WsClient {
public:
	WsClient() noexcept = default;
	~WsClient() noexcept { close(); }

	// Non-copyable
	WsClient(const WsClient&) = delete;
	WsClient& operator=(const WsClient&) = delete;

	// ─── Connection ─────────────────────

	/**
	 * @brief Connect to a WebSocket server via URL
	 * @param url WebSocket URL (ws:// or wss://)
	 * @param config Connection options
	 */
	core::Error connect(std::string_view url,
		WsClientConfig config = {}) noexcept
	{
		config_ = config;

		auto parsed = Url::parse(std::string(url));
		if (parsed.host.empty()) return core::Error::InvalidAddress;

		host_ = parsed.host;
		path_ = parsed.path.empty() ? "/" : parsed.path;
		uint16_t port = parsed.port;
		if (port == 0) {
			port = (parsed.scheme == "wss" || parsed.scheme == "https")
				? 443 : 80;
		}

		// Resolve host
		auto ip = net::Ip<4>(127, 0, 0, 1);
		if (host_ != "localhost" && host_ != "127.0.0.1") {
			auto dns = net::Dns::resolve(host_);
			if (dns.success && !dns.ipv4_addresses.empty()) {
				ip = dns.ipv4_addresses[0];
			} else {
				return core::Error::InvalidAddress;
			}
		}

		// TCP connect
		auto addr = net::SocketAddress<net::Ip<4>>(ip, port);
		if (auto err = socket_.create(); core::is_error(err)) return err;
		socket_.set_timeout(config_.timeout_ms);
		if (auto err = socket_.connect(addr); core::is_error(err)) return err;

		// Generate masking key for handshake
		ws_key_ = generate_ws_key();

		// Send WebSocket upgrade request
		auto request = build_upgrade_request();
		auto req_data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(request.data()), request.size());
		socket_.send(req_data);

		// Read upgrade response
		std::array<uint8_t, 2048> buffer{};
		int received = socket_.recv(buffer);
		if (received <= 0) return core::Error::HandshakeFailed;

		std::string_view response(
			reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		if (response.find("101") == std::string_view::npos) {
			return core::Error::HandshakeFailed;
		}

		connected_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Close the WebSocket connection gracefully
	 */
	void close(WsCloseCode code = WsCloseCode::Normal) noexcept {
		if (connected_) {
			// Send Close frame
			WsFrame close_frame;
			close_frame.opcode = WsOpcode::Close;
			close_frame.fin = true;
			close_frame.masked = true;
			close_frame.mask_key = generate_mask();

			// Close payload: 2-byte status code
			close_frame.payload.push_back(
				static_cast<uint8_t>((static_cast<uint16_t>(code) >> 8) & 0xFF));
			close_frame.payload.push_back(
				static_cast<uint8_t>(static_cast<uint16_t>(code) & 0xFF));

			auto encoded = ws_encode_frame(close_frame);
			socket_.send(std::span<const uint8_t>(encoded.data(), encoded.size()));

			connected_ = false;
		}
		socket_.close();
	}

	// ─── Send ───────────────────────────

	/**
	 * @brief Send a text message
	 */
	core::Error send_text(std::string_view text) {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.set_text(text);
		frame.masked = true;
		frame.mask_key = generate_mask();

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	/**
	 * @brief Send a binary message
	 */
	core::Error send_binary(std::span<const uint8_t> data) {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.set_binary(data);
		frame.masked = true;
		frame.mask_key = generate_mask();

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	/**
	 * @brief Send a Ping frame
	 */
	core::Error send_ping(std::string_view payload = "") {
		if (!connected_) return core::Error::NotConnected;

		WsFrame frame;
		frame.opcode = WsOpcode::Ping;
		frame.fin = true;
		frame.masked = true;
		frame.mask_key = generate_mask();
		if (!payload.empty()) {
			frame.payload.assign(payload.begin(), payload.end());
		}

		auto encoded = ws_encode_frame(frame);
		int sent = socket_.send(
			std::span<const uint8_t>(encoded.data(), encoded.size()));
		return (sent > 0) ? core::Error::None : core::Error::SendFailed;
	}

	// ─── Receive ────────────────────────

	/**
	 * @brief Receive the next message (blocks until a data frame arrives)
	 * 
	 * Automatically handles Ping/Pong and Close frames.
	 * Returns the complete reassembled message for fragmented transfers.
	 */
	std::expected<WsMessage, core::Error> recv() {
		if (!connected_) return std::unexpected(core::Error::NotConnected);

		WsMessage message;
		bool first_frame = true;

		while (true) {
			std::array<uint8_t, 65536> buffer{};
			int received = socket_.recv(buffer);
			if (received <= 0) {
				connected_ = false;
				return std::unexpected(core::Error::ReceiveFailed);
			}

			auto frame = ws_decode_frame(
				std::span<const uint8_t>(buffer.data(),
					static_cast<size_t>(received)));

			switch (frame.opcode) {
				case WsOpcode::Ping:
					if (config_.auto_pong) {
						send_pong(frame.payload);
					}
					continue;

				case WsOpcode::Pong:
					continue;

				case WsOpcode::Close:
					handle_close_frame(frame);
					return std::unexpected(core::Error::SocketClosed);

				case WsOpcode::Text:
				case WsOpcode::Binary:
					if (first_frame) {
						message.opcode = frame.opcode;
						first_frame = false;
					}
					message.data.insert(message.data.end(),
						frame.payload.begin(), frame.payload.end());
					if (frame.fin) {
						message.complete = true;
						return message;
					}
					break;

				case WsOpcode::Continuation:
					message.data.insert(message.data.end(),
						frame.payload.begin(), frame.payload.end());
					if (frame.fin) {
						message.complete = true;
						return message;
					}
					break;

				default:
					break;
			}
		}
	}

	// ─── Queries ────────────────────────

	bool is_connected() const noexcept { return connected_; }

	// ─── Handshake helpers (public for testing) ───

	/**
	 * @brief Build the HTTP upgrade request string
	 */
	std::string build_upgrade_request() const {
		std::string req;
		req += "GET " + path_ + " HTTP/1.1\r\n";
		req += "Host: " + host_ + "\r\n";
		req += "Upgrade: websocket\r\n";
		req += "Connection: Upgrade\r\n";
		req += "Sec-WebSocket-Key: " + ws_key_ + "\r\n";
		req += "Sec-WebSocket-Version: 13\r\n";

		if (!config_.origin.empty()) {
			req += "Origin: " + config_.origin + "\r\n";
		}
		if (!config_.protocol.empty()) {
			req += "Sec-WebSocket-Protocol: " + config_.protocol + "\r\n";
		}
		for (const auto& [key, val] : config_.extra_headers) {
			req += key + ": " + val + "\r\n";
		}

		req += "\r\n";
		return req;
	}

	/**
	 * @brief Generate a random WebSocket key (Base64-encoded 16 bytes)
	 */
	static std::string generate_ws_key() {
		static constexpr std::string_view chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);

		std::string key;
		key.reserve(24);
		for (int i = 0; i < 22; ++i) {
			key += chars[dist(gen)];
		}
		key += "==";
		return key;
	}

	/**
	 * @brief Generate a random 4-byte masking key
	 */
	static std::array<uint8_t, 4> generate_mask() {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<unsigned int> dist(0, 255);
		return {
			static_cast<uint8_t>(dist(gen)),
			static_cast<uint8_t>(dist(gen)),
			static_cast<uint8_t>(dist(gen)),
			static_cast<uint8_t>(dist(gen))
		};
	}

private:
	net::Socket<net::Ip<4>> socket_;
	WsClientConfig          config_;
	std::string             host_;
	std::string             path_;
	std::string             ws_key_;
	bool                    connected_ = false;

	/**
	 * @brief Send a Pong frame in response to a Ping
	 */
	void send_pong(const std::vector<uint8_t>& payload) {
		WsFrame pong;
		pong.opcode = WsOpcode::Pong;
		pong.fin = true;
		pong.masked = true;
		pong.mask_key = generate_mask();
		pong.payload = payload;

		auto encoded = ws_encode_frame(pong);
		socket_.send(std::span<const uint8_t>(encoded.data(), encoded.size()));
	}

	/**
	 * @brief Handle an incoming Close frame
	 */
	void handle_close_frame(
		[[maybe_unused]] const WsFrame& frame)
	{
		if (connected_) {
			// Echo close frame back
			WsFrame close_reply;
			close_reply.opcode = WsOpcode::Close;
			close_reply.fin = true;
			close_reply.masked = true;
			close_reply.mask_key = generate_mask();
			close_reply.payload = frame.payload;

			auto encoded = ws_encode_frame(close_reply);
			socket_.send(
				std::span<const uint8_t>(encoded.data(), encoded.size()));
			connected_ = false;
		}
	}
};

} // namespace protocol
} // namespace etherz

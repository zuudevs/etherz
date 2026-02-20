/**
 * @file websocket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief WebSocket protocol support (RFC 6455)
 * @version 1.0.0
 * @date 2026-02-19
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
#include <print>

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  WebSocket Opcodes
// ═══════════════════════════════════════════════

enum class WsOpcode : uint8_t {
	Continuation = 0x0,
	Text         = 0x1,
	Binary       = 0x2,
	Close        = 0x8,
	Ping         = 0x9,
	Pong         = 0xA
};

inline constexpr std::string_view ws_opcode_name(WsOpcode op) noexcept {
	switch (op) {
		case WsOpcode::Continuation: return "Continuation";
		case WsOpcode::Text:         return "Text";
		case WsOpcode::Binary:       return "Binary";
		case WsOpcode::Close:        return "Close";
		case WsOpcode::Ping:         return "Ping";
		case WsOpcode::Pong:         return "Pong";
		default:                     return "Unknown";
	}
}

// ═══════════════════════════════════════════════
//  WebSocket Frame
// ═══════════════════════════════════════════════

/**
 * @brief Represents a WebSocket frame
 */
struct WsFrame {
	bool     fin    = true;
	WsOpcode opcode = WsOpcode::Text;
	bool     masked = false;
	std::array<uint8_t, 4> mask_key{};
	std::vector<uint8_t> payload;

	/**
	 * @brief Set text payload
	 */
	void set_text(std::string_view text) {
		opcode = WsOpcode::Text;
		payload.assign(text.begin(), text.end());
	}

	/**
	 * @brief Set binary payload
	 */
	void set_binary(std::span<const uint8_t> data) {
		opcode = WsOpcode::Binary;
		payload.assign(data.begin(), data.end());
	}

	/**
	 * @brief Get payload as string (for text frames)
	 */
	std::string payload_text() const {
		return std::string(payload.begin(), payload.end());
	}

	inline void display() const noexcept {
		std::print("WsFrame: opcode={}, fin={}, masked={}, payload_len={}\n",
			ws_opcode_name(opcode), fin, masked, payload.size());
	}
};

// ═══════════════════════════════════════════════
//  Frame Encoding / Decoding
// ═══════════════════════════════════════════════

/**
 * @brief Encode a WebSocket frame into bytes
 */
inline std::vector<uint8_t> ws_encode_frame(const WsFrame& frame) {
	std::vector<uint8_t> out;

	// Byte 0: FIN + opcode
	uint8_t b0 = static_cast<uint8_t>(frame.opcode);
	if (frame.fin) b0 |= 0x80;
	out.push_back(b0);

	// Byte 1: MASK + payload length
	uint64_t len = frame.payload.size();
	uint8_t b1 = frame.masked ? 0x80 : 0x00;

	if (len < 126) {
		b1 |= static_cast<uint8_t>(len);
		out.push_back(b1);
	} else if (len <= 0xFFFF) {
		b1 |= 126;
		out.push_back(b1);
		out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>(len & 0xFF));
	} else {
		b1 |= 127;
		out.push_back(b1);
		for (int i = 7; i >= 0; --i) {
			out.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
		}
	}

	// Masking key
	if (frame.masked) {
		out.insert(out.end(), frame.mask_key.begin(), frame.mask_key.end());
	}

	// Payload (apply mask if needed)
	if (frame.masked) {
		for (size_t i = 0; i < frame.payload.size(); ++i) {
			out.push_back(frame.payload[i] ^ frame.mask_key[i % 4]);
		}
	} else {
		out.insert(out.end(), frame.payload.begin(), frame.payload.end());
	}

	return out;
}

/**
 * @brief Decode a WebSocket frame from bytes
 * @return Parsed frame (payload will be empty if data is insufficient)
 */
inline WsFrame ws_decode_frame(std::span<const uint8_t> data) {
	WsFrame frame;
	if (data.size() < 2) return frame;

	size_t pos = 0;

	// Byte 0
	frame.fin = (data[0] & 0x80) != 0;
	frame.opcode = static_cast<WsOpcode>(data[0] & 0x0F);
	pos++;

	// Byte 1
	frame.masked = (data[1] & 0x80) != 0;
	uint64_t payload_len = data[1] & 0x7F;
	pos++;

	if (payload_len == 126) {
		if (data.size() < pos + 2) return frame;
		payload_len = (static_cast<uint64_t>(data[pos]) << 8)
		            | static_cast<uint64_t>(data[pos + 1]);
		pos += 2;
	} else if (payload_len == 127) {
		if (data.size() < pos + 8) return frame;
		payload_len = 0;
		for (int i = 0; i < 8; ++i) {
			payload_len = (payload_len << 8) | static_cast<uint64_t>(data[pos + i]);
		}
		pos += 8;
	}

	// Masking key
	if (frame.masked) {
		if (data.size() < pos + 4) return frame;
		std::copy_n(data.data() + pos, 4, frame.mask_key.begin());
		pos += 4;
	}

	// Payload
	if (data.size() < pos + payload_len) return frame;
	frame.payload.resize(static_cast<size_t>(payload_len));

	if (frame.masked) {
		for (size_t i = 0; i < payload_len; ++i) {
			frame.payload[i] = data[pos + i] ^ frame.mask_key[i % 4];
		}
	} else {
		std::copy_n(data.data() + pos, static_cast<size_t>(payload_len), frame.payload.begin());
	}

	return frame;
}

// ═══════════════════════════════════════════════
//  WebSocket Handshake Helpers
// ═══════════════════════════════════════════════

/**
 * @brief Generate a WebSocket upgrade request
 */
inline std::string ws_handshake_request(std::string_view host, std::string_view path,
	std::string_view key = "dGhlIHNhbXBsZSBub25jZQ==") {
	std::string s;
	s += "GET " + std::string(path) + " HTTP/1.1\r\n";
	s += "Host: " + std::string(host) + "\r\n";
	s += "Upgrade: websocket\r\n";
	s += "Connection: Upgrade\r\n";
	s += "Sec-WebSocket-Key: " + std::string(key) + "\r\n";
	s += "Sec-WebSocket-Version: 13\r\n";
	s += "\r\n";
	return s;
}

/**
 * @brief Generate a WebSocket upgrade response (101 Switching Protocols)
 */
inline std::string ws_handshake_response(
	std::string_view accept_key = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") {
	std::string s;
	s += "HTTP/1.1 101 Switching Protocols\r\n";
	s += "Upgrade: websocket\r\n";
	s += "Connection: Upgrade\r\n";
	s += "Sec-WebSocket-Accept: " + std::string(accept_key) + "\r\n";
	s += "\r\n";
	return s;
}

} // namespace protocol
} // namespace etherz

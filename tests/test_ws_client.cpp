/**
 * @file test_ws_client.cpp
 * @brief Unit tests for WebSocket client/server — handshake, frames, messages
 */

#include "test_framework.hpp"
#include "protocol/ws_client.hpp"
#include "protocol/ws_server.hpp"

namespace etp = etherz::protocol;

// ═══════════════════════════════════════════════
//  WsClient — Upgrade Request
// ═══════════════════════════════════════════════

TEST(ws_client_upgrade_request_format) {
	etp::WsClient client;
	// We need to set internals via connect (can't easily), so test generate_ws_key
	auto key = etp::WsClient::generate_ws_key();
	ASSERT_EQ(key.size(), 24u);
	// Should end with ==
	ASSERT_EQ(key.substr(22), "==");
}

TEST(ws_client_generate_mask) {
	auto m1 = etp::WsClient::generate_mask();
	auto m2 = etp::WsClient::generate_mask();
	// Very unlikely to be identical
	bool same = (m1[0] == m2[0] && m1[1] == m2[1]
		&& m1[2] == m2[2] && m1[3] == m2[3]);
	ASSERT_TRUE(!same);
}

TEST(ws_client_key_is_base64_like) {
	auto key = etp::WsClient::generate_ws_key();
	// Should only contain Base64 chars
	for (size_t i = 0; i < 22; ++i) {
		char c = key[i];
		bool valid = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '+' || c == '/';
		ASSERT_TRUE(valid);
	}
}

// ═══════════════════════════════════════════════
//  WsServer — Upgrade Response
// ═══════════════════════════════════════════════

TEST(ws_server_upgrade_response_format) {
	auto resp = etp::WsServer::build_upgrade_response();
	ASSERT_TRUE(resp.find("101 Switching Protocols") != std::string::npos);
	ASSERT_TRUE(resp.find("Upgrade: websocket") != std::string::npos);
	ASSERT_TRUE(resp.find("Connection: Upgrade") != std::string::npos);
	ASSERT_TRUE(resp.find("Sec-WebSocket-Accept:") != std::string::npos);
}

TEST(ws_server_upgrade_response_custom_key) {
	auto resp = etp::WsServer::build_upgrade_response("customAcceptKey123=");
	ASSERT_TRUE(resp.find("customAcceptKey123=") != std::string::npos);
}

// ═══════════════════════════════════════════════
//  WsServer — Upgrade Detection
// ═══════════════════════════════════════════════

TEST(ws_server_is_upgrade_request) {
	std::string req =
		"GET /ws HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
		"Sec-WebSocket-Version: 13\r\n\r\n";
	ASSERT_TRUE(etp::WsServer::is_upgrade_request(req));
}

TEST(ws_server_not_upgrade_request) {
	std::string req =
		"GET /index.html HTTP/1.1\r\n"
		"Host: localhost\r\n"
		"Connection: keep-alive\r\n\r\n";
	ASSERT_TRUE(!etp::WsServer::is_upgrade_request(req));
}

// ═══════════════════════════════════════════════
//  Frame Encode/Decode Round Trip
// ═══════════════════════════════════════════════

TEST(ws_frame_text_roundtrip) {
	etp::WsFrame frame;
	frame.set_text("Hello, WebSocket!");
	frame.masked = false;
	frame.fin = true;

	auto encoded = etp::ws_encode_frame(frame);
	auto decoded = etp::ws_decode_frame(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_EQ(decoded.opcode, etp::WsOpcode::Text);
	ASSERT_TRUE(decoded.fin);
	ASSERT_EQ(decoded.payload_text(), "Hello, WebSocket!");
}

TEST(ws_frame_binary_roundtrip) {
	std::vector<uint8_t> data = {0x00, 0x01, 0x02, 0xFF, 0xFE};

	etp::WsFrame frame;
	frame.set_binary(std::span<const uint8_t>(data.data(), data.size()));
	frame.masked = false;
	frame.fin = true;

	auto encoded = etp::ws_encode_frame(frame);
	auto decoded = etp::ws_decode_frame(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_EQ(decoded.opcode, etp::WsOpcode::Binary);
	ASSERT_EQ(decoded.payload.size(), 5u);
	ASSERT_EQ(decoded.payload[0], 0x00);
	ASSERT_EQ(decoded.payload[4], 0xFE);
}

TEST(ws_frame_masked_roundtrip) {
	etp::WsFrame frame;
	frame.set_text("Masked message");
	frame.masked = true;
	frame.mask_key = {0xAA, 0xBB, 0xCC, 0xDD};
	frame.fin = true;

	auto encoded = etp::ws_encode_frame(frame);
	auto decoded = etp::ws_decode_frame(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_EQ(decoded.payload_text(), "Masked message");
	ASSERT_TRUE(decoded.masked);
}

TEST(ws_frame_ping_pong) {
	etp::WsFrame ping;
	ping.opcode = etp::WsOpcode::Ping;
	ping.fin = true;
	ping.masked = false;
	ping.payload = {'h', 'i'};

	auto encoded = etp::ws_encode_frame(ping);
	auto decoded = etp::ws_decode_frame(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_EQ(decoded.opcode, etp::WsOpcode::Ping);
	ASSERT_EQ(decoded.payload.size(), 2u);
	ASSERT_EQ(decoded.payload[0], 'h');
}

TEST(ws_frame_close) {
	etp::WsFrame close;
	close.opcode = etp::WsOpcode::Close;
	close.fin = true;
	close.masked = false;
	// Code 1000 (Normal)
	close.payload = {0x03, 0xE8};

	auto encoded = etp::ws_encode_frame(close);
	auto decoded = etp::ws_decode_frame(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_EQ(decoded.opcode, etp::WsOpcode::Close);
	uint16_t code = (static_cast<uint16_t>(decoded.payload[0]) << 8)
		| decoded.payload[1];
	ASSERT_EQ(code, 1000);
}

// ═══════════════════════════════════════════════
//  WsMessage
// ═══════════════════════════════════════════════

TEST(ws_message_text) {
	etp::WsMessage msg;
	msg.opcode = etp::WsOpcode::Text;
	msg.data = {'T', 'e', 's', 't'};
	msg.complete = true;
	ASSERT_EQ(msg.text(), "Test");
}

TEST(ws_message_binary) {
	etp::WsMessage msg;
	msg.opcode = etp::WsOpcode::Binary;
	msg.data = {0xDE, 0xAD, 0xBE, 0xEF};
	auto span = msg.binary();
	ASSERT_EQ(span.size(), 4u);
	ASSERT_EQ(span[0], 0xDE);
	ASSERT_EQ(span[3], 0xEF);
}

// ═══════════════════════════════════════════════
//  WsCloseCode
// ═══════════════════════════════════════════════

TEST(ws_close_codes) {
	ASSERT_EQ(static_cast<uint16_t>(etp::WsCloseCode::Normal), 1000);
	ASSERT_EQ(static_cast<uint16_t>(etp::WsCloseCode::GoingAway), 1001);
	ASSERT_EQ(static_cast<uint16_t>(etp::WsCloseCode::ProtocolError), 1002);
	ASSERT_EQ(static_cast<uint16_t>(etp::WsCloseCode::InternalError), 1011);
}

// ═══════════════════════════════════════════════
//  WsClientConfig Defaults
// ═══════════════════════════════════════════════

TEST(ws_client_config_defaults) {
	etp::WsClientConfig cfg;
	ASSERT_EQ(cfg.timeout_ms, 10000u);
	ASSERT_TRUE(cfg.auto_pong);
	ASSERT_TRUE(!cfg.auto_reconnect);
	ASSERT_TRUE(cfg.origin.empty());
	ASSERT_TRUE(cfg.protocol.empty());
}

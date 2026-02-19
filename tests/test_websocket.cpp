#include "test_framework.hpp"
#include "protocol/websocket.hpp"

namespace etp = etherz::protocol;

TEST_CASE(ws_frame_text) {
	etp::WsFrame frame;
	frame.set_text("Hello WS!");
	CHECK_TRUE(frame.fin);
	CHECK_EQ(frame.opcode, etp::WsOpcode::Text);
	CHECK_EQ(frame.payload_text(), std::string("Hello WS!"));
}

TEST_CASE(ws_encode_decode_roundtrip) {
	etp::WsFrame frame;
	frame.set_text("Roundtrip Test");
	auto encoded = etp::ws_encode_frame(frame);
	CHECK_TRUE(encoded.size() > 0);

	auto decoded = etp::ws_decode_frame(encoded);
	CHECK_TRUE(decoded.fin);
	CHECK_FALSE(decoded.masked);
	CHECK_EQ(decoded.opcode, etp::WsOpcode::Text);
	CHECK_EQ(decoded.payload_text(), std::string("Roundtrip Test"));
}

TEST_CASE(ws_binary_frame) {
	etp::WsFrame frame;
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	frame.set_binary(data);
	CHECK_EQ(frame.opcode, etp::WsOpcode::Binary);
	CHECK_EQ(frame.payload.size(), 3u);
}

TEST_CASE(ws_encode_size) {
	etp::WsFrame frame;
	frame.set_text("Hi");
	auto encoded = etp::ws_encode_frame(frame);
	// 2 header bytes + 2 payload bytes
	CHECK_EQ(encoded.size(), 4u);
}

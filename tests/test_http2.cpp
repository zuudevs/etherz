#include "test_framework.hpp"
#include "protocol/http2.hpp"

namespace ep = etherz::protocol;

// ─── Frame Type Names ──────────────────

TEST_CASE(http2_frame_type_names) {
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Data), std::string_view("DATA"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Headers), std::string_view("HEADERS"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Priority), std::string_view("PRIORITY"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::RstStream), std::string_view("RST_STREAM"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Settings), std::string_view("SETTINGS"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::PushPromise), std::string_view("PUSH_PROMISE"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Ping), std::string_view("PING"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::GoAway), std::string_view("GOAWAY"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::WindowUpdate), std::string_view("WINDOW_UPDATE"));
	CHECK_EQ(ep::frame_type_name(ep::Http2FrameType::Continuation), std::string_view("CONTINUATION"));
}

// ─── Frame Serialize + Parse Roundtrip ─

TEST_CASE(http2_frame_serialize_parse) {
	ep::Http2Frame frame;
	frame.type = ep::Http2FrameType::Data;
	frame.flags = ep::http2_flags::END_STREAM;
	frame.stream_id = 1;
	frame.payload = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"

	auto wire = frame.serialize();
	CHECK_TRUE(wire.size() >= 9 + 5);

	size_t consumed = 0;
	auto parsed = ep::http2_parse_frame(wire, consumed);
	CHECK_EQ(consumed, wire.size());
	CHECK_EQ(parsed.type, ep::Http2FrameType::Data);
	CHECK_EQ(parsed.stream_id, static_cast<uint32_t>(1));
	CHECK_TRUE(parsed.is_end_stream());
	CHECK_EQ(parsed.payload.size(), static_cast<size_t>(5));
	CHECK_EQ(parsed.payload[0], static_cast<uint8_t>(0x48));
}

// ─── Frame Flags ───────────────────────

TEST_CASE(http2_frame_flags) {
	ep::Http2Frame frame;
	frame.flags = ep::http2_flags::END_STREAM | ep::http2_flags::END_HEADERS;
	CHECK_TRUE(frame.is_end_stream());
	CHECK_TRUE(frame.is_end_headers());
	CHECK_TRUE(frame.has_flag(ep::http2_flags::END_STREAM));
}

// ─── HPACK Roundtrip ──────────────────

TEST_CASE(http2_hpack_roundtrip) {
	ep::HpackCodec::HeaderList headers = {
		{":method", "GET"},
		{":path", "/index.html"},
		{"host", "example.com"}
	};

	auto encoded = ep::HpackCodec::encode(headers);
	CHECK_TRUE(!encoded.empty());

	auto decoded = ep::HpackCodec::decode(encoded);
	CHECK_EQ(decoded.size(), static_cast<size_t>(3));
	CHECK_EQ(decoded[0].first, std::string(":method"));
	CHECK_EQ(decoded[0].second, std::string("GET"));
	CHECK_EQ(decoded[1].first, std::string(":path"));
	CHECK_EQ(decoded[1].second, std::string("/index.html"));
	CHECK_EQ(decoded[2].first, std::string("host"));
	CHECK_EQ(decoded[2].second, std::string("example.com"));
}

// ─── Frame Builders ───────────────────

TEST_CASE(http2_builder_settings_ack) {
	std::vector<std::pair<ep::Http2Setting, uint32_t>> empty;
	auto frame = ep::http2_builder::settings(empty, true);
	CHECK_EQ(frame.type, ep::Http2FrameType::Settings);
	CHECK_TRUE(frame.has_flag(ep::http2_flags::ACK));
	CHECK_EQ(frame.stream_id, static_cast<uint32_t>(0));
	CHECK_TRUE(frame.payload.empty());
}

TEST_CASE(http2_builder_headers) {
	ep::HpackCodec::HeaderList hdrs = {{":method", "POST"}};
	auto frame = ep::http2_builder::headers(3, hdrs, true, true);
	CHECK_EQ(frame.type, ep::Http2FrameType::Headers);
	CHECK_EQ(frame.stream_id, static_cast<uint32_t>(3));
	CHECK_TRUE(frame.is_end_stream());
	CHECK_TRUE(frame.is_end_headers());
	CHECK_TRUE(!frame.payload.empty());
}

TEST_CASE(http2_builder_data) {
	std::vector<uint8_t> payload = {1, 2, 3};
	auto frame = ep::http2_builder::data(5, payload, true);
	CHECK_EQ(frame.type, ep::Http2FrameType::Data);
	CHECK_EQ(frame.stream_id, static_cast<uint32_t>(5));
	CHECK_TRUE(frame.is_end_stream());
	CHECK_EQ(frame.payload.size(), static_cast<size_t>(3));
}

TEST_CASE(http2_builder_ping) {
	auto frame = ep::http2_builder::ping({}, false);
	CHECK_EQ(frame.type, ep::Http2FrameType::Ping);
	CHECK_EQ(frame.stream_id, static_cast<uint32_t>(0));
	CHECK_EQ(frame.payload.size(), static_cast<size_t>(8));
}

TEST_CASE(http2_builder_goaway) {
	auto frame = ep::http2_builder::goaway(7, ep::Http2Error::NoError);
	CHECK_EQ(frame.type, ep::Http2FrameType::GoAway);
	CHECK_EQ(frame.payload.size(), static_cast<size_t>(8));
}

TEST_CASE(http2_builder_window_update) {
	auto frame = ep::http2_builder::window_update(1, 65535);
	CHECK_EQ(frame.type, ep::Http2FrameType::WindowUpdate);
	CHECK_EQ(frame.stream_id, static_cast<uint32_t>(1));
	CHECK_EQ(frame.payload.size(), static_cast<size_t>(4));
}

// ─── Client Preface ───────────────────

TEST_CASE(http2_client_preface) {
	CHECK_EQ(ep::HTTP2_CLIENT_PREFACE,
		std::string_view("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"));
}

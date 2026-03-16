/**
 * @file http2.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief HTTP/2 framing, HPACK header compression, and stream multiplexing
 * @version 2.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <array>
#include <unordered_map>
#include <algorithm>

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  HTTP/2 Frame Types (RFC 7540 §6)
// ═══════════════════════════════════════════════

enum class Http2FrameType : uint8_t {
	Data         = 0x0,
	Headers      = 0x1,
	Priority     = 0x2,
	RstStream    = 0x3,
	Settings     = 0x4,
	PushPromise  = 0x5,
	Ping         = 0x6,
	GoAway       = 0x7,
	WindowUpdate = 0x8,
	Continuation = 0x9
};

inline constexpr std::string_view frame_type_name(Http2FrameType type) noexcept {
	switch (type) {
		case Http2FrameType::Data:         return "DATA";
		case Http2FrameType::Headers:      return "HEADERS";
		case Http2FrameType::Priority:     return "PRIORITY";
		case Http2FrameType::RstStream:    return "RST_STREAM";
		case Http2FrameType::Settings:     return "SETTINGS";
		case Http2FrameType::PushPromise:  return "PUSH_PROMISE";
		case Http2FrameType::Ping:         return "PING";
		case Http2FrameType::GoAway:       return "GOAWAY";
		case Http2FrameType::WindowUpdate: return "WINDOW_UPDATE";
		case Http2FrameType::Continuation: return "CONTINUATION";
		default:                           return "UNKNOWN";
	}
}

// ═══════════════════════════════════════════════
//  HTTP/2 Frame Flags
// ═══════════════════════════════════════════════

namespace http2_flags {
	constexpr uint8_t END_STREAM  = 0x1;
	constexpr uint8_t END_HEADERS = 0x4;
	constexpr uint8_t PADDED      = 0x8;
	constexpr uint8_t PRIORITY    = 0x20;
	constexpr uint8_t ACK         = 0x1;  // For SETTINGS and PING
} // namespace http2_flags

// ═══════════════════════════════════════════════
//  HTTP/2 Settings Parameters (RFC 7540 §6.5.2)
// ═══════════════════════════════════════════════

enum class Http2Setting : uint16_t {
	HeaderTableSize      = 0x1,
	EnablePush           = 0x2,
	MaxConcurrentStreams  = 0x3,
	InitialWindowSize    = 0x4,
	MaxFrameSize         = 0x5,
	MaxHeaderListSize    = 0x6
};

// ═══════════════════════════════════════════════
//  HTTP/2 Error Codes (RFC 7540 §7)
// ═══════════════════════════════════════════════

enum class Http2Error : uint32_t {
	NoError            = 0x0,
	ProtocolError      = 0x1,
	InternalError      = 0x2,
	FlowControlError   = 0x3,
	SettingsTimeout    = 0x4,
	StreamClosed       = 0x5,
	FrameSizeError     = 0x6,
	RefusedStream      = 0x7,
	Cancel             = 0x8,
	CompressionError   = 0x9,
	ConnectError       = 0xA,
	EnhanceYourCalm    = 0xB,
	InadequateSecurity = 0xC,
	Http11Required     = 0xD
};

// ═══════════════════════════════════════════════
//  HTTP/2 Frame
// ═══════════════════════════════════════════════

/**
 * @brief An HTTP/2 frame (RFC 7540 §4)
 * 
 * Frame layout:
 *   Length (24) | Type (8) | Flags (8) | Reserved (1) | Stream ID (31)
 *   Frame Payload (Length bytes)
 */
struct Http2Frame {
	Http2FrameType        type      = Http2FrameType::Data;
	uint8_t               flags     = 0;
	uint32_t              stream_id = 0;
	std::vector<uint8_t>  payload;

	/**
	 * @brief Serialize frame to wire format
	 */
	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> out;
		uint32_t length = static_cast<uint32_t>(payload.size());

		// 9-byte header
		out.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
		out.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>(length & 0xFF));
		out.push_back(static_cast<uint8_t>(type));
		out.push_back(flags);
		out.push_back(static_cast<uint8_t>((stream_id >> 24) & 0x7F));  // Clear reserved bit
		out.push_back(static_cast<uint8_t>((stream_id >> 16) & 0xFF));
		out.push_back(static_cast<uint8_t>((stream_id >> 8) & 0xFF));
		out.push_back(static_cast<uint8_t>(stream_id & 0xFF));

		// Payload
		out.insert(out.end(), payload.begin(), payload.end());
		return out;
	}

	bool has_flag(uint8_t flag) const noexcept { return (flags & flag) != 0; }
	bool is_end_stream() const noexcept { return has_flag(http2_flags::END_STREAM); }
	bool is_end_headers() const noexcept { return has_flag(http2_flags::END_HEADERS); }
};

/**
 * @brief Parse an HTTP/2 frame from raw bytes
 * @param data Raw wire data (must contain at least 9 bytes for header)
 * @param bytes_consumed Output: total bytes consumed (header + payload)
 * @return Parsed frame
 */
inline Http2Frame http2_parse_frame(std::span<const uint8_t> data, size_t& bytes_consumed) {
	Http2Frame frame;
	bytes_consumed = 0;

	if (data.size() < 9) return frame;

	uint32_t length = (static_cast<uint32_t>(data[0]) << 16)
	                | (static_cast<uint32_t>(data[1]) << 8)
	                | static_cast<uint32_t>(data[2]);
	frame.type      = static_cast<Http2FrameType>(data[3]);
	frame.flags     = data[4];
	frame.stream_id = ((static_cast<uint32_t>(data[5]) & 0x7F) << 24)
	                | (static_cast<uint32_t>(data[6]) << 16)
	                | (static_cast<uint32_t>(data[7]) << 8)
	                | static_cast<uint32_t>(data[8]);

	if (data.size() < 9 + length) return frame;

	frame.payload.assign(data.begin() + 9, data.begin() + 9 + length);
	bytes_consumed = 9 + length;
	return frame;
}

// ═══════════════════════════════════════════════
//  HTTP/2 Connection Preface
// ═══════════════════════════════════════════════

/**
 * @brief HTTP/2 client connection preface (magic octets)
 */
inline constexpr std::string_view HTTP2_CLIENT_PREFACE = 
	"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

// ═══════════════════════════════════════════════
//  HPACK Header Compression (simplified)
// ═══════════════════════════════════════════════

/**
 * @brief Simplified HPACK encoder/decoder for HTTP/2 headers
 * 
 * Supports literal header field representation with and without
 * indexing. For a production implementation, full static/dynamic
 * table support would be needed.
 */
class HpackCodec {
public:
	using HeaderList = std::vector<std::pair<std::string, std::string>>;

	/**
	 * @brief Encode headers to HPACK format (literal without indexing)
	 */
	static std::vector<uint8_t> encode(const HeaderList& headers) {
		std::vector<uint8_t> out;
		for (const auto& [name, value] : headers) {
			// Literal Header Field without Indexing (0000 0000)
			out.push_back(0x00);
			encode_string(out, name);
			encode_string(out, value);
		}
		return out;
	}

	/**
	 * @brief Decode HPACK-encoded headers
	 */
	static HeaderList decode(std::span<const uint8_t> data) {
		HeaderList headers;
		size_t pos = 0;

		while (pos < data.size()) {
			uint8_t byte = data[pos];

			if ((byte & 0x80) != 0) {
				// Indexed Header Field — skip (simplified)
				++pos;
				continue;
			}

			if ((byte & 0xC0) == 0x40 || byte == 0x00) {
				// Literal Header Field
				++pos;
				if (byte != 0x00 && (byte & 0x3F) != 0) {
					// Name from index — skip name, decode value
					auto value = decode_string(data, pos);
					headers.emplace_back("", value);
				} else {
					auto name = decode_string(data, pos);
					auto value = decode_string(data, pos);
					headers.emplace_back(name, value);
				}
			} else {
				++pos;  // Skip unhandled representations
			}
		}

		return headers;
	}

private:
	static void encode_string(std::vector<uint8_t>& out, std::string_view str) {
		// No Huffman encoding (bit 7 = 0)
		encode_integer(out, static_cast<uint32_t>(str.size()), 7);
		out.insert(out.end(), str.begin(), str.end());
	}

	static std::string decode_string(std::span<const uint8_t> data, size_t& pos) {
		if (pos >= data.size()) return "";
		bool huffman = (data[pos] & 0x80) != 0;
		uint32_t length = decode_integer(data, pos, 7);
		(void)huffman;  // Huffman decoding not implemented in simplified version

		if (pos + length > data.size()) return "";
		std::string result(reinterpret_cast<const char*>(data.data() + pos), length);
		pos += length;
		return result;
	}

	static void encode_integer(std::vector<uint8_t>& out, uint32_t value, uint8_t prefix_bits) {
		uint8_t max_prefix = static_cast<uint8_t>((1 << prefix_bits) - 1);
		if (value < max_prefix) {
			out.push_back(static_cast<uint8_t>(value));
		} else {
			out.push_back(max_prefix);
			value -= max_prefix;
			while (value >= 128) {
				out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
				value >>= 7;
			}
			out.push_back(static_cast<uint8_t>(value));
		}
	}

	static uint32_t decode_integer(std::span<const uint8_t> data, size_t& pos, uint8_t prefix_bits) {
		if (pos >= data.size()) return 0;
		uint8_t max_prefix = static_cast<uint8_t>((1 << prefix_bits) - 1);
		uint32_t value = data[pos] & max_prefix;
		++pos;

		if (value < max_prefix) return value;

		uint32_t shift = 0;
		while (pos < data.size()) {
			uint8_t byte = data[pos++];
			value += static_cast<uint32_t>(byte & 0x7F) << shift;
			shift += 7;
			if ((byte & 0x80) == 0) break;
		}
		return value;
	}
};

// ═══════════════════════════════════════════════
//  HTTP/2 Stream State
// ═══════════════════════════════════════════════

/**
 * @brief HTTP/2 stream states (RFC 7540 §5.1)
 */
enum class Http2StreamState : uint8_t {
	Idle,
	ReservedLocal,
	ReservedRemote,
	Open,
	HalfClosedLocal,
	HalfClosedRemote,
	Closed
};

/**
 * @brief Represents a single HTTP/2 stream
 */
struct Http2Stream {
	uint32_t                stream_id = 0;
	Http2StreamState        state     = Http2StreamState::Idle;
	int32_t                 window_size = 65535;   // Default initial window
	HpackCodec::HeaderList  request_headers;
	HpackCodec::HeaderList  response_headers;
	std::vector<uint8_t>    data;
};

// ═══════════════════════════════════════════════
//  Frame Builder Helpers
// ═══════════════════════════════════════════════

namespace http2_builder {

/**
 * @brief Build a SETTINGS frame
 */
inline Http2Frame settings(const std::vector<std::pair<Http2Setting, uint32_t>>& params,
	bool ack = false) 
{
	Http2Frame frame;
	frame.type = Http2FrameType::Settings;
	frame.flags = ack ? http2_flags::ACK : 0;
	frame.stream_id = 0;

	if (!ack) {
		for (const auto& [id, value] : params) {
			auto sid = static_cast<uint16_t>(id);
			frame.payload.push_back(static_cast<uint8_t>((sid >> 8) & 0xFF));
			frame.payload.push_back(static_cast<uint8_t>(sid & 0xFF));
			frame.payload.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
			frame.payload.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
			frame.payload.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
			frame.payload.push_back(static_cast<uint8_t>(value & 0xFF));
		}
	}

	return frame;
}

/**
 * @brief Build a HEADERS frame
 */
inline Http2Frame headers(uint32_t stream_id, const HpackCodec::HeaderList& hdrs,
	bool end_stream = false, bool end_headers = true)
{
	Http2Frame frame;
	frame.type = Http2FrameType::Headers;
	frame.stream_id = stream_id;
	frame.flags = 0;
	if (end_stream) frame.flags |= http2_flags::END_STREAM;
	if (end_headers) frame.flags |= http2_flags::END_HEADERS;
	frame.payload = HpackCodec::encode(hdrs);
	return frame;
}

/**
 * @brief Build a DATA frame
 */
inline Http2Frame data(uint32_t stream_id, std::span<const uint8_t> payload,
	bool end_stream = true)
{
	Http2Frame frame;
	frame.type = Http2FrameType::Data;
	frame.stream_id = stream_id;
	frame.flags = end_stream ? http2_flags::END_STREAM : 0;
	frame.payload.assign(payload.begin(), payload.end());
	return frame;
}

/**
 * @brief Build a WINDOW_UPDATE frame
 */
inline Http2Frame window_update(uint32_t stream_id, uint32_t increment) {
	Http2Frame frame;
	frame.type = Http2FrameType::WindowUpdate;
	frame.stream_id = stream_id;
	frame.payload.resize(4);
	frame.payload[0] = static_cast<uint8_t>((increment >> 24) & 0x7F);
	frame.payload[1] = static_cast<uint8_t>((increment >> 16) & 0xFF);
	frame.payload[2] = static_cast<uint8_t>((increment >> 8) & 0xFF);
	frame.payload[3] = static_cast<uint8_t>(increment & 0xFF);
	return frame;
}

/**
 * @brief Build a GOAWAY frame
 */
inline Http2Frame goaway(uint32_t last_stream_id, Http2Error error_code) {
	Http2Frame frame;
	frame.type = Http2FrameType::GoAway;
	frame.stream_id = 0;
	auto e = static_cast<uint32_t>(error_code);
	frame.payload = {
		static_cast<uint8_t>((last_stream_id >> 24) & 0x7F),
		static_cast<uint8_t>((last_stream_id >> 16) & 0xFF),
		static_cast<uint8_t>((last_stream_id >> 8) & 0xFF),
		static_cast<uint8_t>(last_stream_id & 0xFF),
		static_cast<uint8_t>((e >> 24) & 0xFF),
		static_cast<uint8_t>((e >> 16) & 0xFF),
		static_cast<uint8_t>((e >> 8) & 0xFF),
		static_cast<uint8_t>(e & 0xFF)
	};
	return frame;
}

/**
 * @brief Build a PING frame
 */
inline Http2Frame ping(std::array<uint8_t, 8> opaque_data = {}, bool ack = false) {
	Http2Frame frame;
	frame.type = Http2FrameType::Ping;
	frame.stream_id = 0;
	frame.flags = ack ? http2_flags::ACK : 0;
	frame.payload.assign(opaque_data.begin(), opaque_data.end());
	return frame;
}

} // namespace http2_builder

} // namespace protocol
} // namespace etherz

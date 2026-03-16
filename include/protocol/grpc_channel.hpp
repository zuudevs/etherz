/**
 * @file grpc_channel.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief gRPC client channel for making RPC calls
 * @version 2.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <functional>
#include <expected>
#include <span>

#include "http2.hpp"
#include "protobuf.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  gRPC Status Codes
// ═══════════════════════════════════════════════

enum class GrpcStatus : uint8_t {
	Ok                 = 0,
	Cancelled          = 1,
	Unknown            = 2,
	InvalidArgument    = 3,
	DeadlineExceeded   = 4,
	NotFound           = 5,
	AlreadyExists      = 6,
	PermissionDenied   = 7,
	ResourceExhausted  = 8,
	FailedPrecondition = 9,
	Aborted            = 10,
	OutOfRange         = 11,
	Unimplemented      = 12,
	Internal           = 13,
	Unavailable        = 14,
	DataLoss           = 15,
	Unauthenticated    = 16
};

inline constexpr std::string_view grpc_status_name(GrpcStatus status) noexcept {
	switch (status) {
		case GrpcStatus::Ok:                 return "OK";
		case GrpcStatus::Cancelled:          return "CANCELLED";
		case GrpcStatus::Unknown:            return "UNKNOWN";
		case GrpcStatus::InvalidArgument:    return "INVALID_ARGUMENT";
		case GrpcStatus::DeadlineExceeded:   return "DEADLINE_EXCEEDED";
		case GrpcStatus::NotFound:           return "NOT_FOUND";
		case GrpcStatus::AlreadyExists:      return "ALREADY_EXISTS";
		case GrpcStatus::PermissionDenied:   return "PERMISSION_DENIED";
		case GrpcStatus::ResourceExhausted:  return "RESOURCE_EXHAUSTED";
		case GrpcStatus::FailedPrecondition: return "FAILED_PRECONDITION";
		case GrpcStatus::Aborted:            return "ABORTED";
		case GrpcStatus::OutOfRange:         return "OUT_OF_RANGE";
		case GrpcStatus::Unimplemented:      return "UNIMPLEMENTED";
		case GrpcStatus::Internal:           return "INTERNAL";
		case GrpcStatus::Unavailable:        return "UNAVAILABLE";
		case GrpcStatus::DataLoss:           return "DATA_LOSS";
		case GrpcStatus::Unauthenticated:    return "UNAUTHENTICATED";
		default:                             return "UNKNOWN";
	}
}

// ═══════════════════════════════════════════════
//  gRPC Response
// ═══════════════════════════════════════════════

/**
 * @brief Result of a gRPC call
 */
struct GrpcResponse {
	GrpcStatus           status = GrpcStatus::Unknown;
	std::string          status_message;
	std::vector<uint8_t> data;         // Raw protobuf response
	HpackCodec::HeaderList headers;    // Response headers
	HpackCodec::HeaderList trailers;   // Response trailers

	/**
	 * @brief Decode the response data as a ProtoMessage
	 */
	ProtoMessage message() const {
		return ProtoMessage::deserialize(data);
	}

	bool ok() const noexcept { return status == GrpcStatus::Ok; }
};

// ═══════════════════════════════════════════════
//  gRPC Channel
// ═══════════════════════════════════════════════

/**
 * @brief gRPC client channel
 * 
 * Connects to a gRPC server and provides methods for making
 * unary RPC calls over HTTP/2.
 * 
 * Usage:
 *   GrpcChannel channel;
 *   channel.connect("localhost", 50051);
 *   
 *   ProtoMessage request;
 *   request.set_string(1, "Hello");
 *   
 *   auto response = channel.unary_call("/greet.Greeter/SayHello", request);
 */
class GrpcChannel {
public:
	GrpcChannel() noexcept = default;
	~GrpcChannel() noexcept { close(); }

	// Non-copyable
	GrpcChannel(const GrpcChannel&) = delete;
	GrpcChannel& operator=(const GrpcChannel&) = delete;

	/**
	 * @brief Connect to a gRPC server
	 * @param host Server hostname or IP
	 * @param port Server port (default 50051)
	 */
	core::Error connect(std::string_view host, uint16_t port = 50051) noexcept {
		host_ = std::string(host);
		port_ = port;

		// Resolve host
		auto ip = net::Ip<4>(127, 0, 0, 1);
		if (host != "localhost" && host != "127.0.0.1") {
			auto dns = net::Dns::resolve(std::string(host));
			if (dns.success && !dns.ipv4_addresses.empty()) {
				ip = dns.ipv4_addresses[0];
			}
		}

		auto addr = net::SocketAddress<net::Ip<4>>(ip, port);

		if (auto err = socket_.create(); core::is_error(err)) return err;
		if (auto err = socket_.connect(addr); core::is_error(err)) return err;

		// Send HTTP/2 client preface
		auto preface = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(HTTP2_CLIENT_PREFACE.data()),
			HTTP2_CLIENT_PREFACE.size());
		socket_.send(preface);

		// Send initial SETTINGS
		auto settings = http2_builder::settings({
			{Http2Setting::MaxConcurrentStreams, 100},
			{Http2Setting::InitialWindowSize, 65535}
		});
		auto settings_data = settings.serialize();
		socket_.send(std::span<const uint8_t>(settings_data.data(), settings_data.size()));

		connected_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Perform a unary gRPC call
	 * @param method Full method path (e.g., "/package.Service/Method")
	 * @param request Protobuf request message
	 * @return gRPC response
	 */
	std::expected<GrpcResponse, core::Error> unary_call(
		std::string_view method, const ProtoMessage& request)
	{
		if (!connected_) return std::unexpected(core::Error::NotConnected);

		uint32_t stream_id = next_stream_id_;
		next_stream_id_ += 2;  // Client streams are odd-numbered

		// Build HEADERS frame with gRPC metadata
		HpackCodec::HeaderList headers = {
			{":method", "POST"},
			{":scheme", "http"},
			{":path", std::string(method)},
			{":authority", host_ + ":" + std::to_string(port_)},
			{"content-type", "application/grpc"},
			{"te", "trailers"},
			{"grpc-encoding", "identity"}
		};

		auto headers_frame = http2_builder::headers(stream_id, headers, false, true);
		auto headers_data = headers_frame.serialize();
		socket_.send(std::span<const uint8_t>(headers_data.data(), headers_data.size()));

		// Encode and send the request as a DATA frame
		auto proto_bytes = request.serialize();
		auto grpc_framed = grpc_encode_message(proto_bytes);
		auto data_frame = http2_builder::data(stream_id,
			std::span<const uint8_t>(grpc_framed.data(), grpc_framed.size()), true);
		auto data_bytes = data_frame.serialize();
		socket_.send(std::span<const uint8_t>(data_bytes.data(), data_bytes.size()));

		// Read response frames
		return read_response(stream_id);
	}

	/**
	 * @brief Close the gRPC channel
	 */
	void close() noexcept {
		if (connected_) {
			auto goaway = http2_builder::goaway(0, Http2Error::NoError);
			auto goaway_data = goaway.serialize();
			socket_.send(std::span<const uint8_t>(goaway_data.data(), goaway_data.size()));
			connected_ = false;
		}
		socket_.close();
	}

	bool is_connected() const noexcept { return connected_; }

private:
	net::Socket<net::Ip<4>> socket_;
	std::string host_;
	uint16_t port_ = 50051;
	uint32_t next_stream_id_ = 1;
	bool connected_ = false;

	/**
	 * @brief Read and assemble a gRPC response from HTTP/2 frames
	 */
	std::expected<GrpcResponse, core::Error> read_response(uint32_t stream_id) {
		GrpcResponse response;
		std::vector<uint8_t> all_data;
		std::vector<uint8_t> recv_buffer(16384);
		bool got_headers = false;
		bool done = false;

		while (!done) {
			int received = socket_.recv(std::span<uint8_t>(recv_buffer.data(), recv_buffer.size()));
			if (received <= 0) break;

			auto frame_data = std::span<const uint8_t>(recv_buffer.data(),
				static_cast<size_t>(received));
			size_t pos = 0;

			while (pos < frame_data.size()) {
				size_t consumed = 0;
				auto remaining = frame_data.subspan(pos);
				auto frame = http2_parse_frame(remaining, consumed);
				if (consumed == 0) break;
				pos += consumed;

				if (frame.stream_id != stream_id && frame.stream_id != 0) continue;

				switch (frame.type) {
					case Http2FrameType::Headers:
						if (!got_headers) {
							response.headers = HpackCodec::decode(frame.payload);
							got_headers = true;
						} else {
							response.trailers = HpackCodec::decode(frame.payload);
						}
						if (frame.is_end_stream()) done = true;
						break;

					case Http2FrameType::Data:
						all_data.insert(all_data.end(),
							frame.payload.begin(), frame.payload.end());
						if (frame.is_end_stream()) done = true;
						break;

					case Http2FrameType::Settings:
						if (!frame.has_flag(http2_flags::ACK)) {
							// Send SETTINGS ACK
							auto ack = http2_builder::settings({}, true);
							auto ack_data = ack.serialize();
							socket_.send(std::span<const uint8_t>(
								ack_data.data(), ack_data.size()));
						}
						break;

					case Http2FrameType::WindowUpdate:
						break;  // Accept window updates silently

					case Http2FrameType::Ping:
						if (!frame.has_flag(http2_flags::ACK)) {
							std::array<uint8_t, 8> pdata{};
							if (frame.payload.size() >= 8) {
								std::copy_n(frame.payload.begin(), 8, pdata.begin());
							}
							auto pong = http2_builder::ping(pdata, true);
							auto pong_data = pong.serialize();
							socket_.send(std::span<const uint8_t>(
								pong_data.data(), pong_data.size()));
						}
						break;

					case Http2FrameType::GoAway:
						done = true;
						break;

					default:
						break;
				}
			}
		}

		// Decode gRPC response data
		if (!all_data.empty()) {
			bool compressed = false;
			response.data = grpc_decode_message(all_data, compressed);
		}

		// Extract gRPC status from trailers
		response.status = GrpcStatus::Ok;
		for (const auto& [key, value] : response.trailers) {
			if (key == "grpc-status") {
				response.status = static_cast<GrpcStatus>(std::stoi(value));
			} else if (key == "grpc-message") {
				response.status_message = value;
			}
		}

		return response;
	}
};

} // namespace protocol
} // namespace etherz

/**
 * @file grpc_server.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief gRPC server with service registration and dispatch
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
#include <functional>
#include <unordered_map>
#include <print>

#include "http2.hpp"
#include "protobuf.hpp"
#include "grpc_channel.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  gRPC Service Handler Types
// ═══════════════════════════════════════════════

/**
 * @brief gRPC call context (metadata, deadlines, etc.)
 */
struct GrpcContext {
	HpackCodec::HeaderList request_headers;
	std::string peer_address;
};

/**
 * @brief Unary RPC handler
 * 
 * Takes a request ProtoMessage and context, returns a response ProtoMessage and status.
 */
using UnaryHandler = std::function<std::pair<ProtoMessage, GrpcStatus>(
	const ProtoMessage& request, const GrpcContext& ctx)>;

/**
 * @brief Server-streaming RPC handler
 * 
 * Called once per request. The callback sends response messages via the writer.
 */
using ServerStreamWriter = std::function<void(const ProtoMessage& response)>;
using ServerStreamHandler = std::function<GrpcStatus(
	const ProtoMessage& request, const GrpcContext& ctx, ServerStreamWriter writer)>;

// ═══════════════════════════════════════════════
//  gRPC Server
// ═══════════════════════════════════════════════

/**
 * @brief gRPC server with service registration
 * 
 * Listens for HTTP/2 connections and dispatches incoming
 * gRPC requests to registered service handlers.
 * 
 * Usage:
 *   GrpcServer server;
 *   server.register_unary("/greet.Greeter/SayHello",
 *       [](const ProtoMessage& req, const GrpcContext&) {
 *           ProtoMessage resp;
 *           resp.set_string(1, "Hello " + req.get_string(1));
 *           return std::make_pair(resp, GrpcStatus::Ok);
 *       });
 *   server.listen(50051);
 *   server.serve();  // Blocking
 */
class GrpcServer {
public:
	GrpcServer() noexcept = default;
	~GrpcServer() noexcept { stop(); }

	// Non-copyable
	GrpcServer(const GrpcServer&) = delete;
	GrpcServer& operator=(const GrpcServer&) = delete;

	// ─── Service Registration ───────────

	/**
	 * @brief Register a unary RPC handler
	 * @param method Full method path (e.g., "/package.Service/Method")
	 * @param handler Handler function
	 */
	void register_unary(std::string method, UnaryHandler handler) {
		unary_handlers_[std::move(method)] = std::move(handler);
	}

	/**
	 * @brief Register a server-streaming RPC handler
	 */
	void register_server_stream(std::string method, ServerStreamHandler handler) {
		stream_handlers_[std::move(method)] = std::move(handler);
	}

	// ─── Server Lifecycle ───────────────

	/**
	 * @brief Bind and listen on the given port
	 * @param port Port number (default 50051)
	 * @param addr Bind address (default 0.0.0.0)
	 */
	core::Error listen(uint16_t port = 50051,
		const net::Ip<4>& addr = net::Ip<4>(0, 0, 0, 0)) noexcept
	{
		auto bind_addr = net::SocketAddress<net::Ip<4>>(addr, port);

		if (auto err = listener_.create(); core::is_error(err)) return err;
		if (auto err = listener_.set_reuse_addr(true); core::is_error(err)) return err;
		if (auto err = listener_.bind(bind_addr); core::is_error(err)) return err;
		if (auto err = listener_.listen(); core::is_error(err)) return err;

		listening_ = true;
		port_ = port;
		return core::Error::None;
	}

	/**
	 * @brief Handle one incoming connection (blocking)
	 */
	core::Error handle_one() {
		if (!listening_) return core::Error::SocketClosed;

		auto accept_result = listener_.accept();
		if (!accept_result) return accept_result.error();

		auto client = std::move(accept_result->socket);
		return handle_connection(client);
	}

	/**
	 * @brief Serve connections continuously (blocking loop)
	 */
	void serve() {
		running_ = true;
		while (running_ && listening_) {
			auto err = handle_one();
			if (core::is_error(err) && err != core::Error::Timeout) {
				// Log error but continue serving
			}
		}
	}

	/**
	 * @brief Stop the server
	 */
	void stop() noexcept {
		running_ = false;
		listening_ = false;
		listener_.close();
	}

	// ─── Queries ────────────────────────

	bool is_listening() const noexcept { return listening_; }
	size_t handler_count() const noexcept {
		return unary_handlers_.size() + stream_handlers_.size();
	}

private:
	net::Socket<net::Ip<4>> listener_;
	std::unordered_map<std::string, UnaryHandler> unary_handlers_;
	std::unordered_map<std::string, ServerStreamHandler> stream_handlers_;
	uint16_t port_ = 50051;
	bool listening_ = false;
	bool running_ = false;

	/**
	 * @brief Handle a single HTTP/2 + gRPC connection
	 */
	core::Error handle_connection(net::Socket<net::Ip<4>>& client) {
		// Read & validate client preface
		std::array<uint8_t, 24> preface{};
		int received = client.recv(preface);
		if (received < 24) return core::Error::ReceiveFailed;

		auto expected_preface = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(HTTP2_CLIENT_PREFACE.data()),
			HTTP2_CLIENT_PREFACE.size());
		if (!std::equal(preface.begin(), preface.begin() + 24,
			expected_preface.begin())) {
			return core::Error::ReceiveFailed;
		}

		// Send server SETTINGS
		auto settings = http2_builder::settings({
			{Http2Setting::MaxConcurrentStreams, 100}
		});
		auto settings_data = settings.serialize();
		client.send(std::span<const uint8_t>(settings_data.data(), settings_data.size()));

		// Process frames
		std::vector<uint8_t> recv_buffer(16384);
		std::unordered_map<uint32_t, Http2Stream> streams;

		bool connection_alive = true;
		while (connection_alive) {
			int bytes = client.recv(std::span<uint8_t>(recv_buffer.data(), recv_buffer.size()));
			if (bytes <= 0) break;

			auto frame_data = std::span<const uint8_t>(recv_buffer.data(),
				static_cast<size_t>(bytes));
			size_t pos = 0;

			while (pos < frame_data.size()) {
				size_t consumed = 0;
				auto remaining = frame_data.subspan(pos);
				auto frame = http2_parse_frame(remaining, consumed);
				if (consumed == 0) break;
				pos += consumed;

				switch (frame.type) {
					case Http2FrameType::Settings:
						if (!frame.has_flag(http2_flags::ACK)) {
							auto ack = http2_builder::settings({}, true);
							auto ack_data = ack.serialize();
							client.send(std::span<const uint8_t>(
								ack_data.data(), ack_data.size()));
						}
						break;

					case Http2FrameType::Headers: {
						auto& stream = streams[frame.stream_id];
						stream.stream_id = frame.stream_id;
						stream.request_headers = HpackCodec::decode(frame.payload);
						stream.state = Http2StreamState::Open;
						if (frame.is_end_stream()) {
							dispatch_rpc(client, stream);
							stream.state = Http2StreamState::Closed;
						}
						break;
					}

					case Http2FrameType::Data: {
						auto it = streams.find(frame.stream_id);
						if (it != streams.end()) {
							it->second.data.insert(it->second.data.end(),
								frame.payload.begin(), frame.payload.end());
							if (frame.is_end_stream()) {
								dispatch_rpc(client, it->second);
								it->second.state = Http2StreamState::Closed;
							}
						}
						break;
					}

					case Http2FrameType::WindowUpdate:
						break;

					case Http2FrameType::Ping: {
						if (!frame.has_flag(http2_flags::ACK)) {
							std::array<uint8_t, 8> pdata{};
							if (frame.payload.size() >= 8) {
								std::copy_n(frame.payload.begin(), 8, pdata.begin());
							}
							auto pong = http2_builder::ping(pdata, true);
							auto pong_data = pong.serialize();
							client.send(std::span<const uint8_t>(
								pong_data.data(), pong_data.size()));
						}
						break;
					}

					case Http2FrameType::GoAway:
						connection_alive = false;
						break;

					default:
						break;
				}
			}
		}

		client.close();
		return core::Error::None;
	}

	/**
	 * @brief Dispatch a complete gRPC request to the appropriate handler
	 */
	void dispatch_rpc(net::Socket<net::Ip<4>>& client, Http2Stream& stream) {
		// Find the method path from headers
		std::string method;
		for (const auto& [key, value] : stream.request_headers) {
			if (key == ":path") {
				method = value;
				break;
			}
		}

		GrpcContext ctx;
		ctx.request_headers = stream.request_headers;

		// Decode the gRPC request
		bool compressed = false;
		auto request_bytes = grpc_decode_message(stream.data, compressed);
		auto request = ProtoMessage::deserialize(request_bytes);

		// Try unary handlers first
		auto unary_it = unary_handlers_.find(method);
		if (unary_it != unary_handlers_.end()) {
			auto [response, status] = unary_it->second(request, ctx);
			send_unary_response(client, stream.stream_id, response, status);
			return;
		}

		// Try stream handlers
		auto stream_it = stream_handlers_.find(method);
		if (stream_it != stream_handlers_.end()) {
			auto writer = [&](const ProtoMessage& msg) {
				send_data_frame(client, stream.stream_id, msg, false);
			};
			auto status = stream_it->second(request, ctx, writer);
			send_trailers(client, stream.stream_id, status);
			return;
		}

		// Method not found
		ProtoMessage empty;
		send_unary_response(client, stream.stream_id, empty, GrpcStatus::Unimplemented);
	}

	/**
	 * @brief Send a unary gRPC response
	 */
	void send_unary_response(net::Socket<net::Ip<4>>& client, uint32_t stream_id,
		const ProtoMessage& response, GrpcStatus status)
	{
		// Send response HEADERS
		HpackCodec::HeaderList resp_headers = {
			{":status", "200"},
			{"content-type", "application/grpc"},
			{"grpc-encoding", "identity"}
		};
		auto headers_frame = http2_builder::headers(stream_id, resp_headers, false, true);
		auto hdata = headers_frame.serialize();
		client.send(std::span<const uint8_t>(hdata.data(), hdata.size()));

		// Send response DATA
		send_data_frame(client, stream_id, response, true);

		// Send trailers with gRPC status
		send_trailers(client, stream_id, status);
	}

	void send_data_frame(net::Socket<net::Ip<4>>& client, uint32_t stream_id,
		const ProtoMessage& msg, bool end_stream)
	{
		auto proto_bytes = msg.serialize();
		auto grpc_framed = grpc_encode_message(proto_bytes);
		auto frame = http2_builder::data(stream_id,
			std::span<const uint8_t>(grpc_framed.data(), grpc_framed.size()), end_stream);
		auto data = frame.serialize();
		client.send(std::span<const uint8_t>(data.data(), data.size()));
	}

	void send_trailers(net::Socket<net::Ip<4>>& client, uint32_t stream_id,
		GrpcStatus status)
	{
		HpackCodec::HeaderList trailers = {
			{"grpc-status", std::to_string(static_cast<int>(status))}
		};
		auto trailer_frame = http2_builder::headers(stream_id, trailers, true, true);
		auto tdata = trailer_frame.serialize();
		client.send(std::span<const uint8_t>(tdata.data(), tdata.size()));
	}
};

} // namespace protocol
} // namespace etherz

/**
 * @file http_server.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Lightweight HTTP/1.1 server
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
#include <functional>
#include <print>
#include <array>

#include "http.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

/**
 * @brief Request handler signature
 */
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

/**
 * @brief Lightweight synchronous HTTP/1.1 server
 * 
 * Registers route handlers and processes one request per accept cycle.
 */
class HttpServer {
public:
	/**
	 * @brief Register a route handler
	 * @param method HTTP method to match
	 * @param path   Path to match (exact)
	 * @param handler Function to handle the request
	 */
	void route(HttpMethod method, std::string path, HttpHandler handler) {
		routes_.push_back({method, std::move(path), std::move(handler)});
	}

	/// Shorthand route helpers
	void get(std::string path, HttpHandler handler)  { route(HttpMethod::Get, std::move(path), std::move(handler)); }
	void post(std::string path, HttpHandler handler) { route(HttpMethod::Post, std::move(path), std::move(handler)); }

	/**
	 * @brief Bind and listen on the given address
	 * @return Error if bind/listen fails
	 */
	core::Error listen(const net::SocketAddress<net::Ip<4>>& addr) noexcept {
		auto err = listener_.create();
		if (core::is_error(err)) return err;
		err = listener_.set_reuse_addr(true);
		if (core::is_error(err)) return err;
		err = listener_.bind(addr);
		if (core::is_error(err)) return err;
		err = listener_.listen();
		if (core::is_error(err)) return err;
		listening_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Accept and handle a single request (blocking)
	 * @return Error if accept/recv/send fails
	 */
	core::Error handle_one() {
		if (!listening_) return core::Error::SocketClosed;

		auto accept_result = listener_.accept();
		if (core::is_error(accept_result.error)) return accept_result.error;

		// Wrap the accepted fd into a Socket
		net::Socket<net::Ip<4>> client;
		// Move the fd manually (using take_client pattern)
		auto client_sock = accept_result.take_client();

		// Receive request (loop until headers are complete)
		std::string request_data;
		std::array<uint8_t, 8192> buffer{};
		constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB limit

		while (request_data.size() < MAX_REQUEST_SIZE) {
			int received = client_sock.recv(buffer);
			if (received <= 0) break;
			request_data.append(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));

			// Check if we have complete headers
			if (request_data.find("\r\n\r\n") != std::string::npos) break;
		}

		if (request_data.empty()) {
			client_sock.close();
			return core::Error::ReceiveFailed;
		}

		// Parse and route
		auto req = http_parser::parse_request(request_data);
		auto resp = dispatch(req);

		// Send response
		auto raw = resp.serialize();
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		client_sock.send(data);
		client_sock.close();

		return core::Error::None;
	}

	/**
	 * @brief Stop the server
	 */
	void stop() noexcept {
		listening_ = false;
		listener_.close();
	}

	bool is_listening() const noexcept { return listening_; }
	size_t route_count() const noexcept { return routes_.size(); }

private:
	struct Route {
		HttpMethod method;
		std::string path;
		HttpHandler handler;
	};

	std::vector<Route> routes_;
	net::Socket<net::Ip<4>> listener_;
	bool listening_ = false;

	/**
	 * @brief Find and call matching route handler
	 */
	HttpResponse dispatch(const HttpRequest& req) {
		for (const auto& r : routes_) {
			if (r.method == req.method && r.path == req.path) {
				return r.handler(req);
			}
		}
		// 404 Not Found
		HttpResponse resp;
		resp.status = HttpStatus::NotFound;
		resp.headers.set("Content-Type", "text/plain");
		resp.body = "404 Not Found";
		return resp;
	}
};

} // namespace protocol
} // namespace etherz

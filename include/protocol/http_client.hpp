/**
 * @file http_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Simple HTTP/1.1 client
 * @version 0.4.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <print>

#include "url.hpp"
#include "http.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

/**
 * @brief Simple synchronous HTTP/1.1 client
 * 
 * Uses Socket<Ip<4>> to connect, send request, and receive response.
 */
class HttpClient {
public:
	/**
	 * @brief Result of an HTTP operation
	 */
	struct Result {
		HttpResponse response;
		core::Error  error = core::Error::None;
	};

	/**
	 * @brief Perform a GET request
	 */
	Result get(const Url& url) {
		HttpRequest req;
		req.method = HttpMethod::Get;
		req.path = url.path.empty() ? "/" : url.path;
		if (!url.query.empty()) req.path += "?" + url.query;
		req.headers.set("Host", url.host);
		req.headers.set("Connection", "close");
		req.headers.set("User-Agent", "Etherz/0.4.0");
		return send_request(url, req);
	}

	/**
	 * @brief Perform a POST request
	 */
	Result post(const Url& url, std::string body, std::string_view content_type = "application/json") {
		HttpRequest req;
		req.method = HttpMethod::Post;
		req.path = url.path.empty() ? "/" : url.path;
		req.headers.set("Host", url.host);
		req.headers.set("Connection", "close");
		req.headers.set("User-Agent", "Etherz/0.4.0");
		req.headers.set("Content-Type", std::string(content_type));
		req.body = std::move(body);
		return send_request(url, req);
	}

	/**
	 * @brief Send a custom HTTP request
	 */
	Result send_request(const Url& url, const HttpRequest& req) {
		Result result;

		// Resolve host to IP (simplified: only supports direct IP or localhost)
		net::Ip<4> ip;
		if (url.host == "localhost" || url.host == "127.0.0.1") {
			ip = net::Ip<4>(127, 0, 0, 1);
		} else {
			ip = net::Ip<4>{url.host};
		}

		auto addr = net::SocketAddress<net::Ip<4>>(ip, url.port);

		// Connect
		net::Socket<net::Ip<4>> sock;
		result.error = sock.create();
		if (core::is_error(result.error)) return result;

		result.error = sock.connect(addr);
		if (core::is_error(result.error)) return result;

		// Send
		auto raw = req.serialize();
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		int sent = sock.send(data);
		if (sent < 0) {
			result.error = core::Error::SendFailed;
			return result;
		}

		// Receive
		std::string response_data;
		std::array<uint8_t, 4096> buffer{};
		while (true) {
			int received = sock.recv(buffer);
			if (received <= 0) break;
			response_data.append(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));
		}

		sock.close();

		if (response_data.empty()) {
			result.error = core::Error::ReceiveFailed;
			return result;
		}

		result.response = http_parser::parse_response(response_data);
		result.error = core::Error::None;
		return result;
	}
};

} // namespace protocol
} // namespace etherz

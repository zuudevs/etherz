/**
 * @file http_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Simple HTTP/1.1 client
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
#include <print>

#include "url.hpp"
#include "http.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../security/tls_socket.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

/**
 * @brief Simple synchronous HTTP/1.1 client with HTTPS support
 * 
 * Uses Socket<Ip<4>> for HTTP, TlsSocket<Ip<4>> for HTTPS.
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
	 * @brief Perform a GET request (auto-detects HTTP/HTTPS)
	 */
	Result get(const Url& url) {
		HttpRequest req;
		req.method = HttpMethod::Get;
		req.path = url.path.empty() ? "/" : url.path;
		if (!url.query.empty()) req.path += "?" + url.query;
		req.headers.set("Host", url.host);
		req.headers.set("Connection", "close");
		req.headers.set("User-Agent", "Etherz/1.0.0");
		return send_request(url, req);
	}

	/**
	 * @brief Perform a POST request (auto-detects HTTP/HTTPS)
	 */
	Result post(const Url& url, std::string body, std::string_view content_type = "application/json") {
		HttpRequest req;
		req.method = HttpMethod::Post;
		req.path = url.path.empty() ? "/" : url.path;
		req.headers.set("Host", url.host);
		req.headers.set("Connection", "close");
		req.headers.set("User-Agent", "Etherz/0.5.0");
		req.headers.set("Content-Type", std::string(content_type));
		req.headers.set("Content-Length", std::to_string(body.size()));
		req.body = std::move(body);
		return send_request(url, req);
	}

	/**
	 * @brief Send a custom HTTP request
	 * 
	 * Automatically uses TLS for https:// URLs.
	 */
	Result send_request(const Url& url, const HttpRequest& req) {
		if (url.scheme == "https") {
			return send_secure(url, req);
		}
		return send_plain(url, req);
	}

	/**
	 * @brief Check if HTTPS is supported
	 */
	static constexpr bool supports_https() noexcept {
#ifdef _WIN32
		return true;  // SChannel available
#else
		return false; // POSIX TLS not yet implemented
#endif
	}

private:
	/**
	 * @brief Resolve host to IPv4 via DNS
	 * 
	 * Uses Dns::resolve() for hostname lookup, with fallback to
	 * direct IP string parsing for dotted-decimal addresses.
	 */
	static net::Ip<4> resolve_host(const Url& url) noexcept {
		if (url.host == "localhost" || url.host == "127.0.0.1") {
			return net::Ip<4>(127, 0, 0, 1);
		}

		// Try DNS resolution first (handles hostnames like "example.com")
		auto dns_result = net::Dns::resolve(url.host);
		if (dns_result.success && !dns_result.ipv4_addresses.empty()) {
			return dns_result.ipv4_addresses[0];
		}

		// Fallback: try parsing as a raw IP string
		return net::Ip<4>{url.host};
	}

	/**
	 * @brief Send over plain HTTP
	 */
	Result send_plain(const Url& url, const HttpRequest& req) {
		Result result;
		auto addr = net::SocketAddress<net::Ip<4>>(resolve_host(url), url.port);

		net::Socket<net::Ip<4>> sock;
		result.error = sock.create();
		if (core::is_error(result.error)) return result;

		result.error = sock.connect(addr);
		if (core::is_error(result.error)) return result;

		auto raw = req.serialize();
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		int sent = sock.send(data);
		if (sent < 0) { result.error = core::Error::SendFailed; return result; }

		result = receive_response(sock);
		sock.close();
		return result;
	}

	/**
	 * @brief Send over HTTPS using TlsSocket
	 */
	Result send_secure(const Url& url, const HttpRequest& req) {
		Result result;
		auto addr = net::SocketAddress<net::Ip<4>>(resolve_host(url), url.port);

		auto tls_ctx = security::TlsContext::client(url.host);
		security::TlsSocket<net::Ip<4>> tls_sock;

		result.error = tls_sock.create(tls_ctx);
		if (core::is_error(result.error)) return result;

		result.error = tls_sock.connect(addr);
		if (core::is_error(result.error)) return result;

		auto raw = req.serialize();
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
		int sent = tls_sock.send(data);
		if (sent < 0) { result.error = core::Error::SendFailed; return result; }

		std::string response_data;
		std::array<uint8_t, 4096> buffer{};
		while (true) {
			int received = tls_sock.recv(buffer);
			if (received <= 0) break;
			response_data.append(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));
		}

		tls_sock.close();

		if (response_data.empty()) {
			result.error = core::Error::ReceiveFailed;
			return result;
		}

		result.response = http_parser::parse_response(response_data);
		result.error = core::Error::None;
		return result;
	}

	/**
	 * @brief Helper: receive and parse HTTP response from a plain socket
	 */
	template <typename SocketT>
	Result receive_response(SocketT& sock) {
		Result result;
		std::string response_data;
		std::array<uint8_t, 4096> buffer{};
		while (true) {
			int received = sock.recv(buffer);
			if (received <= 0) break;
			response_data.append(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));
		}

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


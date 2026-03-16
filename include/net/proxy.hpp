/**
 * @file proxy.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Proxy support — SOCKS5 and HTTP CONNECT tunneling
 * @version 1.2.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <array>
#include <span>
#include <expected>

#include "socket.hpp"
#include "socket_address.hpp"
#include "internet_protocol.hpp"
#include "dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Proxy Types
// ═══════════════════════════════════════════════

/**
 * @brief Supported proxy protocol types
 */
enum class ProxyType : uint8_t {
	None,           // Direct connection (no proxy)
	Socks5,         // SOCKS5 proxy (RFC 1928)
	HttpConnect     // HTTP CONNECT tunneling
};

/**
 * @brief Proxy configuration
 */
struct ProxyConfig {
	ProxyType   type = ProxyType::None;
	std::string host;
	uint16_t    port = 1080;          // Default SOCKS5 port
	std::string username;             // Optional auth
	std::string password;             // Optional auth

	/**
	 * @brief Create a SOCKS5 proxy config
	 */
	static ProxyConfig socks5(std::string host, uint16_t port = 1080) {
		ProxyConfig cfg;
		cfg.type = ProxyType::Socks5;
		cfg.host = std::move(host);
		cfg.port = port;
		return cfg;
	}

	/**
	 * @brief Create an HTTP CONNECT proxy config
	 */
	static ProxyConfig http_connect(std::string host, uint16_t port = 8080) {
		ProxyConfig cfg;
		cfg.type = ProxyType::HttpConnect;
		cfg.host = std::move(host);
		cfg.port = port;
		return cfg;
	}

	/**
	 * @brief Set authentication credentials
	 */
	ProxyConfig& auth(std::string user, std::string pass) {
		username = std::move(user);
		password = std::move(pass);
		return *this;
	}

	bool has_auth() const noexcept {
		return !username.empty();
	}

	bool is_enabled() const noexcept {
		return type != ProxyType::None && !host.empty();
	}
};

// ═══════════════════════════════════════════════
//  SOCKS5 Client (RFC 1928)
// ═══════════════════════════════════════════════

/**
 * @brief SOCKS5 proxy client
 * 
 * Performs SOCKS5 handshake over an established TCP connection,
 * then tunnels traffic through the proxy to the target.
 */
class Socks5Client {
public:
	/**
	 * @brief Connect to a target through a SOCKS5 proxy
	 * 
	 * Steps:
	 * 1. Connect TCP to the proxy server
	 * 2. Perform SOCKS5 greeting (method negotiation)
	 * 3. Authenticate if required
	 * 4. Send CONNECT request to target
	 * 5. Socket is now tunneled to target
	 * 
	 * @param sock An already-connected socket to the proxy
	 * @param target_host Target hostname or IP
	 * @param target_port Target port
	 * @param config Proxy auth config
	 * @return Error on failure
	 */
	static core::Error handshake(Socket<Ip<4>>& sock,
		std::string_view target_host, uint16_t target_port,
		const ProxyConfig& config = {}) noexcept
	{
		// ── Step 1: Greeting ──────────────
		// Version(5), NumMethods(1-2), Methods
		std::array<uint8_t, 4> greeting{};
		greeting[0] = 0x05;  // SOCKS version 5
		if (config.has_auth()) {
			greeting[1] = 0x02;  // 2 methods
			greeting[2] = 0x00;  // No auth
			greeting[3] = 0x02;  // Username/password
			sock.send(std::span<const uint8_t>(greeting.data(), 4));
		} else {
			greeting[1] = 0x01;  // 1 method
			greeting[2] = 0x00;  // No auth
			sock.send(std::span<const uint8_t>(greeting.data(), 3));
		}

		// ── Step 2: Read server method choice ──
		std::array<uint8_t, 2> method_response{};
		int recv_bytes = sock.recv(method_response);
		if (recv_bytes < 2 || method_response[0] != 0x05) {
			return core::Error::ProxyError;
		}

		uint8_t chosen_method = method_response[1];

		// ── Step 3: Authenticate if needed ──
		if (chosen_method == 0x02) {
			// Username/Password auth (RFC 1929)
			if (!config.has_auth()) return core::Error::ProxyAuthFailed;

			auto err = do_username_auth(sock, config.username, config.password);
			if (core::is_error(err)) return err;
		} else if (chosen_method == 0xFF) {
			return core::Error::ProxyAuthFailed;  // No acceptable methods
		}
		// 0x00 = no auth needed

		// ── Step 4: CONNECT request ──────────
		return send_connect(sock, target_host, target_port);
	}

private:
	/**
	 * @brief SOCKS5 username/password authentication (RFC 1929)
	 */
	static core::Error do_username_auth(Socket<Ip<4>>& sock,
		std::string_view user, std::string_view pass) noexcept
	{
		// Version(1), ULen(1), User, PLen(1), Pass
		std::vector<uint8_t> auth_req;
		auth_req.push_back(0x01);  // Sub-negotiation version
		auth_req.push_back(static_cast<uint8_t>(user.size()));
		auth_req.insert(auth_req.end(), user.begin(), user.end());
		auth_req.push_back(static_cast<uint8_t>(pass.size()));
		auth_req.insert(auth_req.end(), pass.begin(), pass.end());

		sock.send(std::span<const uint8_t>(auth_req.data(), auth_req.size()));

		std::array<uint8_t, 2> auth_resp{};
		int recv_bytes = sock.recv(auth_resp);
		if (recv_bytes < 2 || auth_resp[1] != 0x00) {
			return core::Error::ProxyAuthFailed;
		}
		return core::Error::None;
	}

	/**
	 * @brief Send SOCKS5 CONNECT command
	 */
	static core::Error send_connect(Socket<Ip<4>>& sock,
		std::string_view host, uint16_t port) noexcept
	{
		// VER(5), CMD(CONNECT=1), RSV(0), ATYP, DST.ADDR, DST.PORT
		std::vector<uint8_t> req;
		req.push_back(0x05);  // Version
		req.push_back(0x01);  // CONNECT
		req.push_back(0x00);  // Reserved

		// Use domain name addressing (ATYP=0x03)
		req.push_back(0x03);
		req.push_back(static_cast<uint8_t>(host.size()));
		req.insert(req.end(), host.begin(), host.end());

		// Port (network byte order)
		req.push_back(static_cast<uint8_t>((port >> 8) & 0xFF));
		req.push_back(static_cast<uint8_t>(port & 0xFF));

		sock.send(std::span<const uint8_t>(req.data(), req.size()));

		// Read response: VER, REP, RSV, ATYP, BND.ADDR, BND.PORT
		std::array<uint8_t, 256> resp{};
		int recv_bytes = sock.recv(resp);
		if (recv_bytes < 4 || resp[0] != 0x05) {
			return core::Error::ProxyError;
		}

		// REP field: 0x00 = success
		if (resp[1] != 0x00) {
			return core::Error::ProxyError;
		}

		return core::Error::None;
	}
};

// ═══════════════════════════════════════════════
//  HTTP CONNECT Tunneling
// ═══════════════════════════════════════════════

/**
 * @brief HTTP CONNECT proxy tunneling
 * 
 * Establishes an HTTP tunnel through a proxy server using
 * the CONNECT method. After handshake, the socket carries
 * raw TCP data to the target.
 */
class HttpConnectProxy {
public:
	/**
	 * @brief Perform HTTP CONNECT handshake
	 * 
	 * @param sock Socket already connected to the proxy
	 * @param target_host Target hostname
	 * @param target_port Target port
	 * @param config Proxy config with optional auth
	 * @return Error on failure
	 */
	static core::Error handshake(Socket<Ip<4>>& sock,
		std::string_view target_host, uint16_t target_port,
		const ProxyConfig& config = {}) noexcept
	{
		// Build CONNECT request
		std::string request = "CONNECT " + std::string(target_host)
			+ ":" + std::to_string(target_port) + " HTTP/1.1\r\n";
		request += "Host: " + std::string(target_host) + ":"
			+ std::to_string(target_port) + "\r\n";

		// Add proxy auth header if needed
		if (config.has_auth()) {
			auto credentials = config.username + ":" + config.password;
			request += "Proxy-Authorization: Basic "
				+ base64_encode(credentials) + "\r\n";
		}

		request += "Proxy-Connection: keep-alive\r\n";
		request += "\r\n";

		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(request.data()), request.size());
		int sent = sock.send(data);
		if (sent < 0) return core::Error::ProxyError;

		// Read response
		std::array<uint8_t, 1024> buffer{};
		int received = sock.recv(buffer);
		if (received <= 0) return core::Error::ProxyError;

		// Parse status line (HTTP/1.x 200 ...)
		std::string_view response(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		// Check for "200" status
		auto sp1 = response.find(' ');
		if (sp1 == std::string_view::npos) return core::Error::ProxyError;
		auto sp2 = response.find(' ', sp1 + 1);
		auto status_str = (sp2 != std::string_view::npos)
			? response.substr(sp1 + 1, sp2 - sp1 - 1)
			: response.substr(sp1 + 1);

		if (status_str != "200") {
			if (status_str == "407") return core::Error::ProxyAuthFailed;
			return core::Error::ProxyError;
		}

		return core::Error::None;
	}

private:
	/**
	 * @brief Simple Base64 encoder for proxy auth
	 */
	static std::string base64_encode(std::string_view input) {
		static constexpr std::string_view chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string result;
		result.reserve(((input.size() + 2) / 3) * 4);

		size_t i = 0;
		while (i < input.size()) {
			uint32_t octet_a = static_cast<uint8_t>(input[i++]);
			uint32_t octet_b = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t octet_c = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

			result += chars[(triple >> 18) & 0x3F];
			result += chars[(triple >> 12) & 0x3F];
			result += (i > input.size() + 1) ? '=' : chars[(triple >> 6) & 0x3F];
			result += (i > input.size()) ? '=' : chars[triple & 0x3F];
		}

		return result;
	}
};

// ═══════════════════════════════════════════════
//  Proxy-Aware Connection Helper
// ═══════════════════════════════════════════════

/**
 * @brief Connect to a target through a proxy (or directly)
 * 
 * @param target Target socket address
 * @param target_host Hostname (for SOCKS5 domain addressing)
 * @param proxy Proxy configuration
 * @return Connected socket, or error
 */
inline std::expected<Socket<Ip<4>>, core::Error> connect_through_proxy(
	const SocketAddress<Ip<4>>& target,
	std::string_view target_host,
	const ProxyConfig& proxy)
{
	if (!proxy.is_enabled()) {
		// Direct connection
		Socket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err))
			return std::unexpected(err);
		if (auto err = sock.connect(target); core::is_error(err))
			return std::unexpected(err);
		return sock;
	}

	// Resolve proxy address
	auto proxy_ip = Ip<4>(127, 0, 0, 1);
	if (proxy.host != "localhost" && proxy.host != "127.0.0.1") {
		auto dns = Dns::resolve(proxy.host);
		if (dns.success && !dns.ipv4_addresses.empty()) {
			proxy_ip = dns.ipv4_addresses[0];
		} else {
			proxy_ip = Ip<4>{proxy.host};
		}
	}

	auto proxy_addr = SocketAddress<Ip<4>>(proxy_ip, proxy.port);

	Socket<Ip<4>> sock;
	if (auto err = sock.create(); core::is_error(err))
		return std::unexpected(err);
	if (auto err = sock.connect(proxy_addr); core::is_error(err))
		return std::unexpected(err);

	// Perform proxy handshake
	core::Error err = core::Error::None;
	switch (proxy.type) {
		case ProxyType::Socks5:
			err = Socks5Client::handshake(sock, target_host, target.port(), proxy);
			break;
		case ProxyType::HttpConnect:
			err = HttpConnectProxy::handshake(sock, target_host, target.port(), proxy);
			break;
		default:
			break;
	}

	if (core::is_error(err)) return std::unexpected(err);
	return sock;
}

} // namespace net
} // namespace etherz

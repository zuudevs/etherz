/**
 * @file tls_context.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief TLS/SSL context and configuration
 * @version 0.5.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <print>

namespace etherz {
namespace security {

/**
 * @brief TLS protocol version
 */
enum class TlsMethod : uint8_t {
	SystemDefault,   // Use system default
	Tls12,           // TLS 1.2
	Tls13            // TLS 1.3
};

inline constexpr std::string_view tls_method_name(TlsMethod m) noexcept {
	switch (m) {
		case TlsMethod::SystemDefault: return "System Default";
		case TlsMethod::Tls12:         return "TLS 1.2";
		case TlsMethod::Tls13:         return "TLS 1.3";
		default:                       return "Unknown";
	}
}

/**
 * @brief Certificate verification mode
 */
enum class TlsVerifyMode : uint8_t {
	None,    // No verification (insecure)
	Peer     // Verify peer certificate
};

inline constexpr std::string_view verify_mode_name(TlsVerifyMode m) noexcept {
	switch (m) {
		case TlsVerifyMode::None: return "None";
		case TlsVerifyMode::Peer: return "Peer";
		default:                  return "Unknown";
	}
}

/**
 * @brief TLS connection role
 */
enum class TlsRole : uint8_t {
	Client,
	Server
};

/**
 * @brief TLS configuration context
 * 
 * Lightweight value type holding TLS session parameters.
 * Used by TlsSocket to configure handshake behavior.
 */
class TlsContext {
public:
	TlsContext() noexcept = default;

	/**
	 * @brief Create a client context with a target hostname
	 */
	static TlsContext client(std::string hostname) {
		TlsContext ctx;
		ctx.role_ = TlsRole::Client;
		ctx.hostname_ = std::move(hostname);
		return ctx;
	}

	/**
	 * @brief Create a server context
	 */
	static TlsContext server() {
		TlsContext ctx;
		ctx.role_ = TlsRole::Server;
		return ctx;
	}

	// ─── Configuration ──────────────────

	void set_method(TlsMethod method) noexcept { method_ = method; }
	void set_verify_mode(TlsVerifyMode mode) noexcept { verify_mode_ = mode; }
	void set_certificate_path(std::string path) { cert_path_ = std::move(path); }
	void set_private_key_path(std::string path) { key_path_ = std::move(path); }
	void set_hostname(std::string hostname) { hostname_ = std::move(hostname); }

	// ─── Accessors ──────────────────────

	TlsMethod       method()           const noexcept { return method_; }
	TlsVerifyMode   verify_mode()      const noexcept { return verify_mode_; }
	TlsRole         role()             const noexcept { return role_; }
	std::string_view hostname()        const noexcept { return hostname_; }
	std::string_view certificate_path() const noexcept { return cert_path_; }
	std::string_view private_key_path() const noexcept { return key_path_; }

	inline void display() const noexcept {
		std::print("TlsContext: method={}, verify={}, role={}, host={}\n",
			tls_method_name(method_),
			verify_mode_name(verify_mode_),
			role_ == TlsRole::Client ? "Client" : "Server",
			hostname_.empty() ? "(none)" : hostname_);
	}

private:
	TlsMethod     method_      = TlsMethod::SystemDefault;
	TlsVerifyMode verify_mode_ = TlsVerifyMode::Peer;
	TlsRole       role_        = TlsRole::Client;
	std::string   hostname_;
	std::string   cert_path_;
	std::string   key_path_;
};

} // namespace security
} // namespace etherz

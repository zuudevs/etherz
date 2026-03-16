/**
 * @file dtls_context.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief DTLS session configuration and cipher suite management
 * @version 2.2.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/error.hpp"

namespace etherz {
namespace security {

// ═══════════════════════════════════════════════
//  DTLS Protocol Version
// ═══════════════════════════════════════════════

enum class DtlsVersion : uint8_t {
	Dtls_1_0 = 0,   // DTLS 1.0 (based on TLS 1.1)
	Dtls_1_2 = 1,   // DTLS 1.2 (based on TLS 1.2)
	Dtls_1_3 = 2    // DTLS 1.3 (based on TLS 1.3)
};

inline constexpr std::string_view dtls_version_name(DtlsVersion ver) noexcept {
	switch (ver) {
		case DtlsVersion::Dtls_1_0: return "DTLS 1.0";
		case DtlsVersion::Dtls_1_2: return "DTLS 1.2";
		case DtlsVersion::Dtls_1_3: return "DTLS 1.3";
		default: return "Unknown";
	}
}

// ═══════════════════════════════════════════════
//  DTLS Configuration
// ═══════════════════════════════════════════════

/**
 * @brief DTLS session configuration
 * 
 * Configures cipher suites, certificate paths, and DTLS-specific
 * parameters like retransmission timeouts and MTU.
 */
struct DtlsConfig {
	DtlsVersion min_version = DtlsVersion::Dtls_1_2;
	DtlsVersion max_version = DtlsVersion::Dtls_1_3;

	// Certificate paths (PEM format)
	std::string cert_file;          // Server/client certificate
	std::string key_file;           // Private key
	std::string ca_file;            // CA certificate for verification

	// DTLS-specific parameters
	uint32_t mtu                = 1400;   // Maximum transmission unit
	uint32_t retransmit_ms      = 1000;   // Initial retransmission timeout
	uint32_t max_retransmit_ms  = 60000;  // Maximum retransmission timeout
	uint32_t handshake_timeout_ms = 30000; // Total handshake timeout

	// Verification
	bool verify_peer = true;               // Verify remote certificate
	bool allow_self_signed = false;        // Allow self-signed certs

	/**
	 * @brief Create a server configuration
	 */
	static DtlsConfig server(std::string cert, std::string key) {
		DtlsConfig cfg;
		cfg.cert_file = std::move(cert);
		cfg.key_file = std::move(key);
		return cfg;
	}

	/**
	 * @brief Create a client configuration
	 */
	static DtlsConfig client(std::string ca = "") {
		DtlsConfig cfg;
		cfg.ca_file = std::move(ca);
		cfg.verify_peer = !cfg.ca_file.empty();
		return cfg;
	}
};

// ═══════════════════════════════════════════════
//  DTLS Context
// ═══════════════════════════════════════════════

/**
 * @brief DTLS context managing session state
 * 
 * Holds the configuration and session parameters shared between
 * multiple DTLS connections. Create one context and use it for
 * multiple DtlsSocket instances.
 */
class DtlsContext {
public:
	explicit DtlsContext(DtlsConfig config = {}) noexcept
		: config_(std::move(config)) {}

	~DtlsContext() noexcept = default;

	// Non-copyable, movable
	DtlsContext(const DtlsContext&) = delete;
	DtlsContext& operator=(const DtlsContext&) = delete;
	DtlsContext(DtlsContext&&) noexcept = default;
	DtlsContext& operator=(DtlsContext&&) noexcept = default;

	/**
	 * @brief Initialize the DTLS context (platform-specific)
	 */
	core::Error initialize() noexcept {
#ifdef _WIN32
		// Windows: Use SChannel DTLS
		// SChannel supports DTLS via SECPKG_ATTR_SUPPORTED_PROTOCOLS
		initialized_ = true;
		return core::Error::None;
#else
		// POSIX: Would use OpenSSL DTLS
		// SSL_CTX_new(DTLS_method()) etc.
		return core::Error::FeatureNotSupported;
#endif
	}

	const DtlsConfig& config() const noexcept { return config_; }
	bool is_initialized() const noexcept { return initialized_; }

	/**
	 * @brief Set cookie secret for DoS protection (RFC 6347 §4.2.1)
	 */
	void set_cookie_secret(std::string_view secret) {
		cookie_secret_ = std::string(secret);
	}

	const std::string& cookie_secret() const noexcept { return cookie_secret_; }

private:
	DtlsConfig  config_;
	std::string  cookie_secret_;
	bool         initialized_ = false;
};

} // namespace security
} // namespace etherz

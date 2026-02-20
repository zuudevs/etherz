/**
 * @file certificate.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Certificate information utilities
 * @version 1.0.0
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
 * @brief Certificate information container
 * 
 * Lightweight representation of an X.509 certificate's key fields.
 * Does not own any system handles.
 */
struct CertInfo {
	std::string subject;      // e.g. "CN=example.com"
	std::string issuer;       // e.g. "CN=Let's Encrypt Authority X3"
	std::string not_before;   // Validity start (ISO 8601 string)
	std::string not_after;    // Validity end
	std::string serial;       // Serial number (hex string)
	std::string fingerprint;  // SHA-256 fingerprint (hex string)
	uint16_t    key_bits = 0; // Key size in bits (e.g. 2048, 256)

	/**
	 * @brief Check if this cert info has been populated
	 */
	bool valid() const noexcept {
		return !subject.empty();
	}

	/**
	 * @brief Pretty print certificate details
	 */
	inline void display() const noexcept {
		std::print("Certificate:\n");
		std::print("  Subject    : {}\n", subject.empty() ? "(empty)" : subject);
		std::print("  Issuer     : {}\n", issuer.empty() ? "(empty)" : issuer);
		std::print("  Valid from : {}\n", not_before.empty() ? "(unknown)" : not_before);
		std::print("  Valid until: {}\n", not_after.empty() ? "(unknown)" : not_after);
		std::print("  Serial     : {}\n", serial.empty() ? "(unknown)" : serial);
		if (!fingerprint.empty())
			std::print("  Fingerprint: {}\n", fingerprint);
		if (key_bits > 0)
			std::print("  Key size   : {} bits\n", key_bits);
	}
};

/**
 * @brief Create a self-signed certificate info (for testing/demo)
 */
inline CertInfo make_self_signed_info(std::string_view common_name,
	uint16_t key_bits = 2048) {
	CertInfo info;
	info.subject     = "CN=" + std::string(common_name);
	info.issuer      = info.subject; // Self-signed
	info.not_before  = "2026-01-01T00:00:00Z";
	info.not_after   = "2027-01-01T00:00:00Z";
	info.serial      = "01";
	info.fingerprint = "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
	info.key_bits    = key_bits;
	return info;
}

} // namespace security
} // namespace etherz

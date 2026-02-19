/**
 * @file subnet.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Subnet / CIDR utilities
 * @version 0.6.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <format>
#include <print>

#include "internet_protocol.hpp"

namespace etherz {
namespace net {

/**
 * @brief IPv4 subnet with CIDR prefix length
 * 
 * Represents a network like 192.168.1.0/24.
 * Supports containment checks, network/broadcast computation, and host counting.
 */
template <typename T>
class Subnet;

template <>
class Subnet<Ip<4>> {
public:
	Subnet() noexcept = default;

	/**
	 * @brief Create subnet from network address and prefix length
	 * @param network Base network address
	 * @param prefix CIDR prefix length (0-32)
	 */
	Subnet(Ip<4> network, uint8_t prefix) noexcept
		: network_(network), prefix_(prefix > 32 ? 32 : prefix) {}

	/**
	 * @brief Parse CIDR notation string (e.g. "192.168.1.0/24")
	 */
	static Subnet parse(std::string_view cidr) {
		auto slash = cidr.find('/');
		if (slash == std::string_view::npos) {
			return Subnet(Ip<4>{cidr}, 32);
		}

		auto ip_str = cidr.substr(0, slash);
		auto prefix_str = cidr.substr(slash + 1);

		uint8_t prefix = 0;
		for (char c : prefix_str) {
			if (c < '0' || c > '9') break;
			prefix = prefix * 10 + (c - '0');
		}

		return Subnet(Ip<4>{ip_str}, prefix);
	}

	// ─── Subnet properties ─────────────

	/**
	 * @brief Get the subnet mask as an IP address
	 */
	Ip<4> mask() const noexcept {
		uint32_t m = mask_bits();
		return Ip<4>(
			static_cast<uint8_t>((m >> 24) & 0xFF),
			static_cast<uint8_t>((m >> 16) & 0xFF),
			static_cast<uint8_t>((m >> 8) & 0xFF),
			static_cast<uint8_t>(m & 0xFF)
		);
	}

	/**
	 * @brief Get the network address (base)
	 */
	Ip<4> network() const noexcept {
		uint32_t net = to_uint32(network_) & mask_bits();
		return from_uint32(net);
	}

	/**
	 * @brief Get the broadcast address
	 */
	Ip<4> broadcast() const noexcept {
		uint32_t bcast = to_uint32(network_) | ~mask_bits();
		return from_uint32(bcast);
	}

	/**
	 * @brief Check if an IP address is within this subnet
	 */
	bool contains(const Ip<4>& ip) const noexcept {
		return (to_uint32(ip) & mask_bits()) == (to_uint32(network_) & mask_bits());
	}

	/**
	 * @brief Number of usable host addresses
	 */
	uint32_t host_count() const noexcept {
		if (prefix_ >= 31) return (prefix_ == 32) ? 1 : 2;
		return (1u << (32 - prefix_)) - 2;
	}

	/**
	 * @brief Get prefix length
	 */
	uint8_t prefix_length() const noexcept { return prefix_; }

	/**
	 * @brief Get the stored network address (may not be aligned)
	 */
	const Ip<4>& address() const noexcept { return network_; }

	/**
	 * @brief Format as CIDR string
	 */
	std::string to_string() const {
		auto b = network().bytes();
		return std::format("{}.{}.{}.{}/{}",
			b[0], b[1], b[2], b[3], prefix_);
	}

	inline void display() const noexcept {
		auto b = network().bytes();
		std::print("Subnet: {}.{}.{}.{}/{} (mask=",
			b[0], b[1], b[2], b[3], prefix_);
		mask().display();
		std::print(", hosts={})\n", host_count());
	}

private:
	Ip<4>   network_;
	uint8_t prefix_ = 0;

	/**
	 * @brief Compute mask as uint32 from prefix
	 */
	uint32_t mask_bits() const noexcept {
		if (prefix_ == 0) return 0;
		return ~((1u << (32 - prefix_)) - 1);
	}

	/**
	 * @brief Convert Ip<4> to uint32 (host order)
	 */
	static uint32_t to_uint32(const Ip<4>& ip) noexcept {
		auto b = ip.bytes();
		return (static_cast<uint32_t>(b[0]) << 24)
		     | (static_cast<uint32_t>(b[1]) << 16)
		     | (static_cast<uint32_t>(b[2]) << 8)
		     | static_cast<uint32_t>(b[3]);
	}

	/**
	 * @brief Convert uint32 (host order) to Ip<4>
	 */
	static Ip<4> from_uint32(uint32_t v) noexcept {
		return Ip<4>(
			static_cast<uint8_t>((v >> 24) & 0xFF),
			static_cast<uint8_t>((v >> 16) & 0xFF),
			static_cast<uint8_t>((v >> 8) & 0xFF),
			static_cast<uint8_t>(v & 0xFF)
		);
	}
};

} // namespace net
} // namespace etherz

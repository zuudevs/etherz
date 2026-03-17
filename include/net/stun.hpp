/**
 * @file stun.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief STUN client for NAT type detection and public IP discovery (RFC 5389)
 * @version 3.1.0
 * @date 2026-03-17
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
#include <random>
#include <cstring>

#include "udp_socket.hpp"
#include "socket_address.hpp"
#include "internet_protocol.hpp"
#include "dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  STUN Constants (RFC 5389)
// ═══════════════════════════════════════════════

namespace stun_detail {
	inline constexpr uint16_t STUN_PORT            = 3478;
	inline constexpr uint32_t STUN_MAGIC_COOKIE    = 0x2112A442;
	inline constexpr uint16_t STUN_BINDING_REQUEST = 0x0001;
	inline constexpr uint16_t STUN_BINDING_RESPONSE = 0x0101;
	inline constexpr uint16_t STUN_BINDING_ERROR    = 0x0111;

	// Attribute types
	inline constexpr uint16_t ATTR_MAPPED_ADDRESS     = 0x0001;
	inline constexpr uint16_t ATTR_CHANGE_REQUEST     = 0x0003;
	inline constexpr uint16_t ATTR_SOURCE_ADDRESS     = 0x0004;
	inline constexpr uint16_t ATTR_CHANGED_ADDRESS    = 0x0005;
	inline constexpr uint16_t ATTR_XOR_MAPPED_ADDRESS = 0x0020;
	inline constexpr uint16_t ATTR_OTHER_ADDRESS      = 0x802C;

	// Address family
	inline constexpr uint8_t FAMILY_IPV4 = 0x01;
	inline constexpr uint8_t FAMILY_IPV6 = 0x02;

	// STUN header size: 20 bytes
	inline constexpr size_t HEADER_SIZE = 20;
} // namespace stun_detail

// ═══════════════════════════════════════════════
//  NAT Type
// ═══════════════════════════════════════════════

/**
 * @brief Detected NAT type
 */
enum class NatType : uint8_t {
	Open,              // No NAT — direct public IP
	FullCone,          // Full cone NAT — any external host can send
	Restricted,        // Restricted cone — only hosts we sent to can reply
	PortRestricted,    // Port-restricted — only same host:port can reply
	Symmetric,         // Symmetric NAT — different mapping per destination
	Unknown            // Could not determine
};

inline constexpr std::string_view nat_type_string(NatType type) noexcept {
	switch (type) {
		case NatType::Open:           return "Open (No NAT)";
		case NatType::FullCone:       return "Full Cone NAT";
		case NatType::Restricted:     return "Restricted Cone NAT";
		case NatType::PortRestricted: return "Port Restricted Cone NAT";
		case NatType::Symmetric:      return "Symmetric NAT";
		case NatType::Unknown:        return "Unknown";
	}
	return "Unknown";
}

// ═══════════════════════════════════════════════
//  STUN Result
// ═══════════════════════════════════════════════

/**
 * @brief Result from a STUN binding request
 */
struct StunResult {
	Ip<4>       public_ip;         // Mapped (public) IP address
	uint16_t    public_port = 0;   // Mapped (public) port
	NatType     nat_type = NatType::Unknown;
	bool        success  = false;

	std::string display() const {
		std::string s;
		s += public_ip.display();
		s += ":" + std::to_string(public_port);
		s += " (" + std::string(nat_type_string(nat_type)) + ")";
		return s;
	}
};

// ═══════════════════════════════════════════════
//  STUN Packet Helpers
// ═══════════════════════════════════════════════

namespace stun_detail {

/**
 * @brief Generate a 96-bit transaction ID
 */
inline std::array<uint8_t, 12> generate_transaction_id() {
	std::array<uint8_t, 12> id{};
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<unsigned int> dist(0, 255);
	for (auto& byte : id) {
		byte = static_cast<uint8_t>(dist(gen));
	}
	return id;
}

/**
 * @brief Build a STUN Binding Request packet
 */
inline std::vector<uint8_t> build_binding_request(
	const std::array<uint8_t, 12>& transaction_id)
{
	std::vector<uint8_t> packet(HEADER_SIZE);

	// Message Type: Binding Request (0x0001)
	packet[0] = 0x00;
	packet[1] = 0x01;

	// Message Length: 0 (no attributes)
	packet[2] = 0x00;
	packet[3] = 0x00;

	// Magic Cookie: 0x2112A442
	packet[4] = 0x21;
	packet[5] = 0x12;
	packet[6] = 0xA4;
	packet[7] = 0x42;

	// Transaction ID: 12 bytes
	std::memcpy(packet.data() + 8, transaction_id.data(), 12);

	return packet;
}

struct MappedAddress {
	Ip<4>    ip;
	uint16_t port = 0;
	bool     valid = false;
};

/**
 * @brief Parse a STUN Binding Response and extract mapped address
 */
inline MappedAddress parse_binding_response(
	std::span<const uint8_t> data,
	const std::array<uint8_t, 12>& transaction_id)
{
	MappedAddress result;

	if (data.size() < HEADER_SIZE) return result;

	// Verify message type: Binding Response (0x0101)
	uint16_t msg_type = (static_cast<uint16_t>(data[0]) << 8) | data[1];
	if (msg_type != STUN_BINDING_RESPONSE) return result;

	// Verify magic cookie
	uint32_t cookie = (static_cast<uint32_t>(data[4]) << 24)
		| (static_cast<uint32_t>(data[5]) << 16)
		| (static_cast<uint32_t>(data[6]) << 8)
		| data[7];
	if (cookie != STUN_MAGIC_COOKIE) return result;

	// Verify transaction ID
	if (std::memcmp(data.data() + 8, transaction_id.data(), 12) != 0) {
		return result;
	}

	// Message length
	uint16_t msg_len = (static_cast<uint16_t>(data[2]) << 8) | data[3];
	if (HEADER_SIZE + msg_len > data.size()) return result;

	// Parse attributes
	size_t pos = HEADER_SIZE;
	while (pos + 4 <= HEADER_SIZE + msg_len) {
		uint16_t attr_type = (static_cast<uint16_t>(data[pos]) << 8)
			| data[pos + 1];
		uint16_t attr_len = (static_cast<uint16_t>(data[pos + 2]) << 8)
			| data[pos + 3];
		pos += 4;

		if (pos + attr_len > data.size()) break;

		if (attr_type == ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8) {
			// XOR-MAPPED-ADDRESS: family(1) pad(1) port(2) ip(4)
			uint8_t family = data[pos + 1];
			if (family == FAMILY_IPV4) {
				uint16_t xor_port = (static_cast<uint16_t>(data[pos + 2]) << 8)
					| data[pos + 3];
				result.port = xor_port ^ static_cast<uint16_t>(
					(STUN_MAGIC_COOKIE >> 16) & 0xFFFF);

				uint32_t xor_ip = (static_cast<uint32_t>(data[pos + 4]) << 24)
					| (static_cast<uint32_t>(data[pos + 5]) << 16)
					| (static_cast<uint32_t>(data[pos + 6]) << 8)
					| data[pos + 7];
				uint32_t mapped_ip = xor_ip ^ STUN_MAGIC_COOKIE;
				result.ip = Ip<4>::from_uint32(mapped_ip);
				result.valid = true;
				return result;
			}
		} else if (attr_type == ATTR_MAPPED_ADDRESS && attr_len >= 8
			&& !result.valid)
		{
			// MAPPED-ADDRESS (fallback if no XOR-MAPPED-ADDRESS)
			uint8_t family = data[pos + 1];
			if (family == FAMILY_IPV4) {
				result.port = (static_cast<uint16_t>(data[pos + 2]) << 8)
					| data[pos + 3];
				result.ip = Ip<4>(data[pos + 4], data[pos + 5],
					data[pos + 6], data[pos + 7]);
				result.valid = true;
				// Don't return yet — prefer XOR-MAPPED-ADDRESS
			}
		}

		// Pad to 4-byte boundary
		pos += attr_len;
		if (attr_len % 4 != 0) {
			pos += 4 - (attr_len % 4);
		}
	}

	return result;
}

} // namespace stun_detail

// ═══════════════════════════════════════════════
//  STUN Client
// ═══════════════════════════════════════════════

/**
 * @brief STUN client for NAT type detection and public IP discovery
 * 
 * Performs STUN Binding Requests (RFC 5389) to discover the public
 * IP address and port as seen by the STUN server.
 * 
 * Usage:
 *   auto result = StunClient::query("stun.l.google.com");
 *   if (result.success) {
 *       std::print("Public IP: {}:{}\n",
 *           result.public_ip.display(), result.public_port);
 *       std::print("NAT Type: {}\n", nat_type_string(result.nat_type));
 *   }
 */
class StunClient {
public:
	/**
	 * @brief Well-known public STUN servers
	 */
	static constexpr std::string_view DEFAULT_SERVER = "stun.l.google.com";

	/**
	 * @brief Perform a STUN binding request to discover public IP
	 * @param server STUN server hostname
	 * @param port STUN server port (default 19302 for Google)
	 * @param timeout_ms Response timeout in milliseconds
	 * @return StunResult with public IP and NAT type
	 */
	static StunResult query(std::string_view server = DEFAULT_SERVER,
		uint16_t port = 19302, uint32_t timeout_ms = 3000)
	{
		StunResult result;

		// Resolve STUN server
		auto ip = resolve_server(server);
		if (ip.to_uint32() == 0) {
			return result;
		}

		auto server_addr = SocketAddress<Ip<4>>(ip, port);

		// Create UDP socket and bind to any port
		UdpSocket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err)) return result;
		sock.set_timeout(timeout_ms);

		// Send Binding Request
		auto txn_id = stun_detail::generate_transaction_id();
		auto request = stun_detail::build_binding_request(txn_id);

		auto data = std::span<const uint8_t>(request.data(), request.size());
		sock.send_to(data, server_addr);

		// Receive response
		std::array<uint8_t, 1024> buffer{};
		auto recv = sock.recv_from(buffer);
		if (recv.bytes_received <= 0) return result;

		// Parse response
		auto mapped = stun_detail::parse_binding_response(
			std::span<const uint8_t>(buffer.data(),
				static_cast<size_t>(recv.bytes_received)),
			txn_id);

		if (mapped.valid) {
			result.public_ip   = mapped.ip;
			result.public_port = mapped.port;
			result.success     = true;

			// Simple NAT type heuristic
			result.nat_type = detect_nat_type(sock, server_addr, txn_id, mapped);
		}

		return result;
	}

	/**
	 * @brief Quick query to just get the public IP (no NAT type detection)
	 */
	static std::expected<Ip<4>, core::Error> get_public_ip(
		std::string_view server = DEFAULT_SERVER,
		uint16_t port = 19302)
	{
		auto result = query(server, port);
		if (!result.success) {
			return std::unexpected(core::Error::StunError);
		}
		return result.public_ip;
	}

	// ─── Packet helpers (public for testing) ───

	/**
	 * @brief Build a STUN binding request packet
	 */
	static std::vector<uint8_t> build_request() {
		auto txn_id = stun_detail::generate_transaction_id();
		return stun_detail::build_binding_request(txn_id);
	}

	/**
	 * @brief Build a STUN binding request with specific transaction ID
	 */
	static std::vector<uint8_t> build_request(
		const std::array<uint8_t, 12>& txn_id)
	{
		return stun_detail::build_binding_request(txn_id);
	}

	/**
	 * @brief Parse a STUN response
	 */
	static stun_detail::MappedAddress parse_response(
		std::span<const uint8_t> data,
		const std::array<uint8_t, 12>& txn_id)
	{
		return stun_detail::parse_binding_response(data, txn_id);
	}

private:
	/**
	 * @brief Resolve STUN server hostname to IP
	 */
	static Ip<4> resolve_server(std::string_view server) {
		if (server == "localhost" || server == "127.0.0.1") {
			return Ip<4>(127, 0, 0, 1);
		}

		auto dns = Dns::resolve(std::string(server));
		if (dns.success && !dns.ipv4_addresses.empty()) {
			return dns.ipv4_addresses[0];
		}

		// Try direct parse
		return Ip<4>{server};
	}

	/**
	 * @brief Detect NAT type using basic heuristic
	 * 
	 * Full RFC 5780 NAT behavior discovery requires multiple STUN servers.
	 * This performs a simplified detection:
	 * - If mapped IP matches local IP → Open
	 * - Otherwise → conservatively reports FullCone
	 */
	static NatType detect_nat_type(
		[[maybe_unused]] UdpSocket<Ip<4>>& sock,
		[[maybe_unused]] const SocketAddress<Ip<4>>& server,
		[[maybe_unused]] const std::array<uint8_t, 12>& txn_id,
		[[maybe_unused]] const stun_detail::MappedAddress& first_mapped)
	{
		// Check if we're directly on a public IP
		// by comparing with local interfaces
		auto interfaces = list_interfaces();
		for (const auto& iface : interfaces) {
			for (const auto& addr : iface.ipv4_addresses) {
				if (addr == first_mapped.ip) {
					return NatType::Open;
				}
			}
		}

		// Without a second STUN server or CHANGE-REQUEST support,
		// we can't distinguish cone types. Report FullCone as default.
		return NatType::FullCone;
	}
};

} // namespace net
} // namespace etherz

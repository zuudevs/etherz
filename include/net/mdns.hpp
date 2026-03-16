/**
 * @file mdns.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Multicast DNS (mDNS) responder and query (RFC 6762)
 * @version 2.5.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <functional>
#include <unordered_map>

#include "internet_protocol.hpp"
#include "socket_address.hpp"
#include "udp_socket.hpp"
#include "multicast_socket.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  DNS Record Types
// ═══════════════════════════════════════════════

enum class DnsRecordType : uint16_t {
	A     = 1,     // IPv4 address
	AAAA  = 28,    // IPv6 address
	PTR   = 12,    // Pointer (reverse lookup)
	SRV   = 33,    // Service locator
	TXT   = 16,    // Text record
	ANY   = 255    // Any type
};

// ═══════════════════════════════════════════════
//  DNS Resource Record
// ═══════════════════════════════════════════════

/**
 * @brief A DNS resource record
 */
struct DnsRecord {
	std::string    name;
	DnsRecordType  type    = DnsRecordType::A;
	uint16_t       class_  = 1;       // IN (Internet)
	uint32_t       ttl     = 120;     // Seconds
	std::vector<uint8_t> rdata;

	// Convenience: set A record data from an IPv4 address
	void set_ipv4(const Ip<4>& ip) {
		type = DnsRecordType::A;
		rdata.resize(4);
		auto octets = ip.octets();
		rdata[0] = octets[0]; rdata[1] = octets[1];
		rdata[2] = octets[2]; rdata[3] = octets[3];
	}

	// Get A record as IPv4
	Ip<4> get_ipv4() const {
		if (rdata.size() >= 4) {
			return Ip<4>(rdata[0], rdata[1], rdata[2], rdata[3]);
		}
		return Ip<4>(0, 0, 0, 0);
	}
};

// ═══════════════════════════════════════════════
//  mDNS Constants
// ═══════════════════════════════════════════════

namespace mdns {
	constexpr uint16_t PORT = 5353;
	inline const Ip<4> MULTICAST_ADDR_V4 = Ip<4>(224, 0, 0, 251);
	constexpr std::string_view LOCAL_DOMAIN = ".local";
} // namespace mdns

// ═══════════════════════════════════════════════
//  DNS Name Encoding/Decoding
// ═══════════════════════════════════════════════

/**
 * @brief Encode a DNS name into wire format
 * Example: "myhost.local" → "\x06myhost\x05local\x00"
 */
inline std::vector<uint8_t> dns_encode_name(std::string_view name) {
	std::vector<uint8_t> out;
	size_t start = 0;
	while (start < name.size()) {
		auto dot = name.find('.', start);
		if (dot == std::string_view::npos) dot = name.size();
		size_t len = dot - start;
		out.push_back(static_cast<uint8_t>(len));
		for (size_t i = start; i < dot; ++i) {
			out.push_back(static_cast<uint8_t>(name[i]));
		}
		start = dot + 1;
	}
	out.push_back(0);  // Root label
	return out;
}

/**
 * @brief Decode a DNS name from wire format
 */
inline std::string dns_decode_name(std::span<const uint8_t> data, size_t& pos) {
	std::string name;
	while (pos < data.size()) {
		uint8_t len = data[pos++];
		if (len == 0) break;
		if ((len & 0xC0) == 0xC0) {
			// Pointer (compression) — follow it
			if (pos >= data.size()) break;
			size_t ptr = ((static_cast<size_t>(len) & 0x3F) << 8) | data[pos++];
			auto suffix = dns_decode_name(data, ptr);
			if (!name.empty()) name += ".";
			name += suffix;
			return name;
		}
		if (!name.empty()) name += ".";
		if (pos + len > data.size()) break;
		name.append(reinterpret_cast<const char*>(data.data() + pos), len);
		pos += len;
	}
	return name;
}

// ═══════════════════════════════════════════════
//  mDNS Query
// ═══════════════════════════════════════════════

/**
 * @brief Send mDNS queries and collect responses
 */
class MdnsQuery {
public:
	using ResultCallback = std::function<void(const DnsRecord&, const Ip<4>&)>;

	MdnsQuery() noexcept = default;

	/**
	 * @brief Query for a hostname on the local network
	 * @param name Hostname to resolve (e.g., "mydevice.local")
	 * @param type Record type (A, AAAA, PTR, SRV, TXT)
	 * @param callback Called for each response received
	 */
	core::Error query(std::string_view name, DnsRecordType type,
		ResultCallback callback)
	{
		MulticastSocket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err)) return err;
		if (auto err = sock.bind(SocketAddress<Ip<4>>(Ip<4>(0,0,0,0), mdns::PORT));
			core::is_error(err)) return err;
		sock.join_group(mdns::MULTICAST_ADDR_V4);

		// Build DNS query packet
		auto query_pkt = build_query(name, type);
		auto dest = SocketAddress<Ip<4>>(mdns::MULTICAST_ADDR_V4, mdns::PORT);
		sock.send_to(std::span<const uint8_t>(query_pkt.data(), query_pkt.size()), dest);

		// Read responses
		std::array<uint8_t, 4096> buffer{};
		sock.set_timeout(2000);  // 2 second timeout
		auto result = sock.recv_from(buffer);
		if (result.bytes > 0) {
			auto records = parse_response(
				std::span<const uint8_t>(buffer.data(),
					static_cast<size_t>(result.bytes)));
			for (const auto& record : records) {
				callback(record, result.sender.ip());
			}
		}

		sock.leave_group(mdns::MULTICAST_ADDR_V4);
		return core::Error::None;
	}

private:
	std::vector<uint8_t> build_query(std::string_view name, DnsRecordType type) {
		std::vector<uint8_t> pkt;
		// Transaction ID (0 for mDNS)
		pkt.push_back(0); pkt.push_back(0);
		// Flags (standard query)
		pkt.push_back(0); pkt.push_back(0);
		// Questions = 1
		pkt.push_back(0); pkt.push_back(1);
		// Answers, Authority, Additional = 0
		pkt.push_back(0); pkt.push_back(0);
		pkt.push_back(0); pkt.push_back(0);
		pkt.push_back(0); pkt.push_back(0);

		// Question
		auto encoded_name = dns_encode_name(name);
		pkt.insert(pkt.end(), encoded_name.begin(), encoded_name.end());
		auto t = static_cast<uint16_t>(type);
		pkt.push_back(static_cast<uint8_t>((t >> 8) & 0xFF));
		pkt.push_back(static_cast<uint8_t>(t & 0xFF));
		pkt.push_back(0); pkt.push_back(1);  // Class IN

		return pkt;
	}

	std::vector<DnsRecord> parse_response(std::span<const uint8_t> data) {
		std::vector<DnsRecord> records;
		if (data.size() < 12) return records;

		uint16_t answers = (static_cast<uint16_t>(data[6]) << 8) | data[7];
		size_t pos = 12;

		// Skip questions
		uint16_t questions = (static_cast<uint16_t>(data[4]) << 8) | data[5];
		for (uint16_t i = 0; i < questions && pos < data.size(); ++i) {
			dns_decode_name(data, pos);
			pos += 4;  // Type + Class
		}

		// Parse answers
		for (uint16_t i = 0; i < answers && pos < data.size(); ++i) {
			DnsRecord record;
			record.name = dns_decode_name(data, pos);
			if (pos + 10 > data.size()) break;

			record.type = static_cast<DnsRecordType>(
				(static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
			record.class_ = (static_cast<uint16_t>(data[pos + 2]) << 8) | data[pos + 3];
			record.ttl = (static_cast<uint32_t>(data[pos + 4]) << 24)
			           | (static_cast<uint32_t>(data[pos + 5]) << 16)
			           | (static_cast<uint32_t>(data[pos + 6]) << 8)
			           | data[pos + 7];
			uint16_t rdlength = (static_cast<uint16_t>(data[pos + 8]) << 8) | data[pos + 9];
			pos += 10;

			if (pos + rdlength <= data.size()) {
				record.rdata.assign(data.begin() + pos, data.begin() + pos + rdlength);
			}
			pos += rdlength;

			records.push_back(std::move(record));
		}

		return records;
	}
};

// ═══════════════════════════════════════════════
//  mDNS Responder
// ═══════════════════════════════════════════════

/**
 * @brief mDNS responder that answers queries for local services
 */
class MdnsResponder {
public:
	MdnsResponder() noexcept = default;

	/**
	 * @brief Register a hostname with its IP address
	 */
	void register_host(std::string name, const Ip<4>& ip) {
		DnsRecord record;
		record.name = std::move(name);
		record.set_ipv4(ip);
		records_.push_back(std::move(record));
	}

	/**
	 * @brief Register a TXT record
	 */
	void register_txt(std::string name, std::string txt) {
		DnsRecord record;
		record.name = std::move(name);
		record.type = DnsRecordType::TXT;
		record.rdata.push_back(static_cast<uint8_t>(txt.size()));
		record.rdata.insert(record.rdata.end(), txt.begin(), txt.end());
		records_.push_back(std::move(record));
	}

	/**
	 * @brief Start responding to mDNS queries (blocking)
	 */
	core::Error serve() {
		MulticastSocket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err)) return err;
		if (auto err = sock.bind(SocketAddress<Ip<4>>(Ip<4>(0,0,0,0), mdns::PORT));
			core::is_error(err)) return err;
		sock.join_group(mdns::MULTICAST_ADDR_V4);

		running_ = true;
		while (running_) {
			std::array<uint8_t, 4096> buffer{};
			auto result = sock.recv_from(buffer);
			if (result.bytes > 0) {
				handle_query(sock,
					std::span<const uint8_t>(buffer.data(),
						static_cast<size_t>(result.bytes)),
					result.sender);
			}
		}

		sock.leave_group(mdns::MULTICAST_ADDR_V4);
		return core::Error::None;
	}

	void stop() noexcept { running_ = false; }

private:
	std::vector<DnsRecord> records_;
	bool running_ = false;

	void handle_query(MulticastSocket<Ip<4>>& sock,
		std::span<const uint8_t> data,
		const SocketAddress<Ip<4>>& sender)
	{
		if (data.size() < 12) return;
		// Check if it's a query (QR=0)
		if ((data[2] & 0x80) != 0) return;

		uint16_t questions = (static_cast<uint16_t>(data[4]) << 8) | data[5];
		size_t pos = 12;

		for (uint16_t i = 0; i < questions && pos < data.size(); ++i) {
			std::string qname = dns_decode_name(data, pos);
			if (pos + 4 > data.size()) break;
			auto qtype = static_cast<DnsRecordType>(
				(static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
			pos += 4;

			// Find matching records and respond
			for (const auto& record : records_) {
				if (record.name == qname &&
					(record.type == qtype || qtype == DnsRecordType::ANY)) {
					auto response = build_response(data, record);
					auto dest = SocketAddress<Ip<4>>(mdns::MULTICAST_ADDR_V4, mdns::PORT);
					sock.send_to(std::span<const uint8_t>(
						response.data(), response.size()), dest);
				}
			}
		}
	}

	std::vector<uint8_t> build_response(std::span<const uint8_t> query,
		const DnsRecord& record)
	{
		std::vector<uint8_t> pkt;
		// Copy transaction ID from query
		pkt.push_back(query[0]); pkt.push_back(query[1]);
		// Flags: response, authoritative
		pkt.push_back(0x84); pkt.push_back(0x00);
		// Questions = 0, Answers = 1
		pkt.push_back(0); pkt.push_back(0);
		pkt.push_back(0); pkt.push_back(1);
		pkt.push_back(0); pkt.push_back(0);
		pkt.push_back(0); pkt.push_back(0);

		// Answer RR
		auto name = dns_encode_name(record.name);
		pkt.insert(pkt.end(), name.begin(), name.end());
		auto t = static_cast<uint16_t>(record.type);
		pkt.push_back(static_cast<uint8_t>((t >> 8) & 0xFF));
		pkt.push_back(static_cast<uint8_t>(t & 0xFF));
		pkt.push_back(0x80); pkt.push_back(0x01);  // Class IN + cache flush
		// TTL
		pkt.push_back(static_cast<uint8_t>((record.ttl >> 24) & 0xFF));
		pkt.push_back(static_cast<uint8_t>((record.ttl >> 16) & 0xFF));
		pkt.push_back(static_cast<uint8_t>((record.ttl >> 8) & 0xFF));
		pkt.push_back(static_cast<uint8_t>(record.ttl & 0xFF));
		// RDLENGTH + RDATA
		uint16_t rdlen = static_cast<uint16_t>(record.rdata.size());
		pkt.push_back(static_cast<uint8_t>((rdlen >> 8) & 0xFF));
		pkt.push_back(static_cast<uint8_t>(rdlen & 0xFF));
		pkt.insert(pkt.end(), record.rdata.begin(), record.rdata.end());

		return pkt;
	}
};

} // namespace net
} // namespace etherz

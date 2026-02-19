/**
 * @file dns.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief DNS resolution utilities
 * @version 0.6.0
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

#include "internet_protocol.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif

namespace etherz {
namespace net {

/**
 * @brief DNS resolution result
 */
struct DnsResult {
	std::vector<Ip<4>> ipv4_addresses;
	std::vector<Ip<6>> ipv6_addresses;
	std::string        canonical_name;
	bool               success = false;

	/**
	 * @brief Total number of resolved addresses
	 */
	size_t count() const noexcept {
		return ipv4_addresses.size() + ipv6_addresses.size();
	}

	inline void display() const noexcept {
		std::print("DNS Result: {} address(es), canonical={}\n",
			count(), canonical_name.empty() ? "(none)" : canonical_name);
		for (const auto& ip : ipv4_addresses) {
			std::print("  IPv4: ");
			ip.display();
		}
		for (const auto& ip : ipv6_addresses) {
			std::print("  IPv6: ");
			ip.display();
		}
	}
};

/**
 * @brief DNS resolution utilities
 * 
 * Uses platform getaddrinfo/getnameinfo for cross-platform DNS.
 */
class Dns {
public:
	/**
	 * @brief Resolve hostname to IP addresses
	 * @param hostname Domain name or IP string
	 * @return DnsResult with resolved addresses
	 */
	static DnsResult resolve(std::string_view hostname) {
		DnsResult result;

		addrinfo hints{};
		hints.ai_family = AF_UNSPEC;      // Both IPv4 and IPv6
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_CANONNAME;

		std::string host_str(hostname);
		addrinfo* res = nullptr;

		int status = ::getaddrinfo(host_str.c_str(), nullptr, &hints, &res);
		if (status != 0 || !res) {
			result.success = false;
			return result;
		}

		// Extract canonical name
		if (res->ai_canonname) {
			result.canonical_name = res->ai_canonname;
		}

		// Collect addresses
		for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
			if (p->ai_family == AF_INET) {
				auto* sa = reinterpret_cast<sockaddr_in*>(p->ai_addr);
				auto addr = ntohl(sa->sin_addr.s_addr);
				result.ipv4_addresses.emplace_back(
					static_cast<uint8_t>((addr >> 24) & 0xFF),
					static_cast<uint8_t>((addr >> 16) & 0xFF),
					static_cast<uint8_t>((addr >> 8) & 0xFF),
					static_cast<uint8_t>(addr & 0xFF)
				);
			} else if (p->ai_family == AF_INET6) {
				auto* sa6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
				std::array<uint16_t, 8> segments{};
				for (int i = 0; i < 8; ++i) {
					segments[i] = ntohs(reinterpret_cast<uint16_t*>(
						&sa6->sin6_addr)[i]);
				}
				result.ipv6_addresses.emplace_back(
					segments[0], segments[1], segments[2], segments[3],
					segments[4], segments[5], segments[6], segments[7]
				);
			}
		}

		::freeaddrinfo(res);
		result.success = true;
		return result;
	}

	/**
	 * @brief Resolve hostname for IPv4 only
	 */
	static DnsResult resolve4(std::string_view hostname) {
		DnsResult result;

		addrinfo hints{};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		std::string host_str(hostname);
		addrinfo* res = nullptr;

		int status = ::getaddrinfo(host_str.c_str(), nullptr, &hints, &res);
		if (status != 0 || !res) {
			result.success = false;
			return result;
		}

		for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
			if (p->ai_family == AF_INET) {
				auto* sa = reinterpret_cast<sockaddr_in*>(p->ai_addr);
				auto addr = ntohl(sa->sin_addr.s_addr);
				result.ipv4_addresses.emplace_back(
					static_cast<uint8_t>((addr >> 24) & 0xFF),
					static_cast<uint8_t>((addr >> 16) & 0xFF),
					static_cast<uint8_t>((addr >> 8) & 0xFF),
					static_cast<uint8_t>(addr & 0xFF)
				);
			}
		}

		::freeaddrinfo(res);
		result.success = true;
		return result;
	}

	/**
	 * @brief Resolve hostname for IPv6 only
	 */
	static DnsResult resolve6(std::string_view hostname) {
		DnsResult result;

		addrinfo hints{};
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_STREAM;

		std::string host_str(hostname);
		addrinfo* res = nullptr;

		int status = ::getaddrinfo(host_str.c_str(), nullptr, &hints, &res);
		if (status != 0 || !res) {
			result.success = false;
			return result;
		}

		for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
			if (p->ai_family == AF_INET6) {
				auto* sa6 = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
				std::array<uint16_t, 8> segments{};
				for (int i = 0; i < 8; ++i) {
					segments[i] = ntohs(reinterpret_cast<uint16_t*>(
						&sa6->sin6_addr)[i]);
				}
				result.ipv6_addresses.emplace_back(
					segments[0], segments[1], segments[2], segments[3],
					segments[4], segments[5], segments[6], segments[7]
				);
			}
		}

		::freeaddrinfo(res);
		result.success = true;
		return result;
	}

	/**
	 * @brief Reverse DNS lookup — IP to hostname
	 * @param ip IPv4 address to look up
	 * @return Hostname string, or empty on failure
	 */
	static std::string reverse(const Ip<4>& ip) {
		sockaddr_in sa{};
		sa.sin_family = AF_INET;
		auto b = ip.bytes();
		sa.sin_addr.s_addr = htonl(
			(static_cast<uint32_t>(b[0]) << 24) |
			(static_cast<uint32_t>(b[1]) << 16) |
			(static_cast<uint32_t>(b[2]) << 8)  |
			static_cast<uint32_t>(b[3])
		);

		char host[NI_MAXHOST]{};
		int status = ::getnameinfo(
			reinterpret_cast<sockaddr*>(&sa), sizeof(sa),
			host, sizeof(host),
			nullptr, 0,
			0
		);

		if (status != 0) return {};
		return std::string(host);
	}
};

} // namespace net
} // namespace etherz

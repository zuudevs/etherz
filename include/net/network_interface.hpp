/**
 * @file network_interface.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Network interface enumeration
 * @version 0.6.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <format>
#include <print>

#include "internet_protocol.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <iphlpapi.h>
	#pragma comment(lib, "iphlpapi.lib")
#else
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

namespace etherz {
namespace net {

/**
 * @brief Represents a network interface on the local machine
 */
struct NetworkInterface {
	std::string        name;          // Interface name (e.g. "Ethernet", "Wi-Fi")
	uint32_t           index = 0;     // Interface index
	std::vector<Ip<4>> ipv4_addresses;
	std::vector<Ip<6>> ipv6_addresses;
	std::array<uint8_t, 6> mac{};     // MAC address (6 bytes)
	bool               is_up = false;
	bool               is_loopback = false;

	/**
	 * @brief Format MAC address as XX:XX:XX:XX:XX:XX
	 */
	std::string mac_string() const {
		return std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	inline void display() const noexcept {
		std::print("Interface: {} (index={})\n", name, index);
		std::print("  MAC      : {}\n", mac_string());
		std::print("  Status   : {}{}\n",
			is_up ? "UP" : "DOWN",
			is_loopback ? " (loopback)" : "");
		for (const auto& ip : ipv4_addresses) {
			std::print("  IPv4     : ");
			ip.display();
		}
		for (const auto& ip : ipv6_addresses) {
			std::print("  IPv6     : ");
			ip.display();
		}
	}
};

/**
 * @brief Enumerate all network interfaces on the local machine
 * @return Vector of NetworkInterface structs
 */
inline std::vector<NetworkInterface> list_interfaces() {
	std::vector<NetworkInterface> result;

#ifdef _WIN32
	ULONG buf_size = 15000;
	std::vector<uint8_t> buffer(buf_size);

	DWORD ret = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX,
		nullptr,
		reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
		&buf_size
	);

	if (ret == ERROR_BUFFER_OVERFLOW) {
		buffer.resize(buf_size);
		ret = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX,
			nullptr,
			reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
			&buf_size
		);
	}

	if (ret != NO_ERROR) return result;

	auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
	while (adapter) {
		NetworkInterface iface;

		// Name (convert wide string)
		if (adapter->FriendlyName) {
			int len = WideCharToMultiByte(CP_UTF8, 0,
				adapter->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
			if (len > 0) {
				std::string name(len - 1, '\0');
				WideCharToMultiByte(CP_UTF8, 0,
					adapter->FriendlyName, -1, name.data(), len, nullptr, nullptr);
				iface.name = std::move(name);
			}
		}

		iface.index = adapter->IfIndex;
		iface.is_up = (adapter->OperStatus == IfOperStatusUp);

		// MAC address
		if (adapter->PhysicalAddressLength >= 6) {
			for (int i = 0; i < 6; ++i) {
				iface.mac[i] = adapter->PhysicalAddress[i];
			}
		}

		// Check loopback
		iface.is_loopback = (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);

		// Unicast addresses
		auto* unicast = adapter->FirstUnicastAddress;
		while (unicast) {
			auto* sa = unicast->Address.lpSockaddr;
			if (sa->sa_family == AF_INET) {
				auto* sa4 = reinterpret_cast<sockaddr_in*>(sa);
				auto addr = ntohl(sa4->sin_addr.s_addr);
				iface.ipv4_addresses.emplace_back(
					static_cast<uint8_t>((addr >> 24) & 0xFF),
					static_cast<uint8_t>((addr >> 16) & 0xFF),
					static_cast<uint8_t>((addr >> 8) & 0xFF),
					static_cast<uint8_t>(addr & 0xFF)
				);
			} else if (sa->sa_family == AF_INET6) {
				auto* sa6 = reinterpret_cast<sockaddr_in6*>(sa);
				std::array<uint16_t, 8> segments{};
				for (int i = 0; i < 8; ++i) {
					segments[i] = ntohs(reinterpret_cast<uint16_t*>(
						&sa6->sin6_addr)[i]);
				}
				iface.ipv6_addresses.emplace_back(
					segments[0], segments[1], segments[2], segments[3],
					segments[4], segments[5], segments[6], segments[7]
				);
			}
			unicast = unicast->Next;
		}

		result.push_back(std::move(iface));
		adapter = adapter->Next;
	}

#else
	// POSIX — getifaddrs
	ifaddrs* ifaddr = nullptr;
	if (::getifaddrs(&ifaddr) == -1) return result;

	// Collect unique interface names
	std::vector<std::string> seen_names;
	for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
		std::string name = ifa->ifa_name;
		bool found = false;
		for (const auto& n : seen_names) {
			if (n == name) { found = true; break; }
		}
		if (!found) seen_names.push_back(name);
	}

	for (const auto& name : seen_names) {
		NetworkInterface iface;
		iface.name = name;

		for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
			if (name != ifa->ifa_name || !ifa->ifa_addr) continue;

			iface.is_up = (ifa->ifa_flags & IFF_UP) != 0;
			iface.is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

			if (ifa->ifa_addr->sa_family == AF_INET) {
				auto* sa = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
				auto addr = ntohl(sa->sin_addr.s_addr);
				iface.ipv4_addresses.emplace_back(
					static_cast<uint8_t>((addr >> 24) & 0xFF),
					static_cast<uint8_t>((addr >> 16) & 0xFF),
					static_cast<uint8_t>((addr >> 8) & 0xFF),
					static_cast<uint8_t>(addr & 0xFF)
				);
			}
		}

		result.push_back(std::move(iface));
	}

	::freeifaddrs(ifaddr);
#endif

	return result;
}

} // namespace net
} // namespace etherz

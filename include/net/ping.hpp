/**
 * @file ping.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief ICMP ping utility
 * @version 0.6.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <print>

#include "internet_protocol.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <iphlpapi.h>
	#include <icmpapi.h>
	#pragma comment(lib, "iphlpapi.lib")
#endif

namespace etherz {
namespace net {

/**
 * @brief Status of a ping operation
 */
enum class PingStatus : uint8_t {
	Success,
	Timeout,
	Unreachable,
	Error
};

inline constexpr std::string_view ping_status_name(PingStatus s) noexcept {
	switch (s) {
		case PingStatus::Success:     return "Success";
		case PingStatus::Timeout:     return "Timeout";
		case PingStatus::Unreachable: return "Unreachable";
		case PingStatus::Error:       return "Error";
		default:                      return "Unknown";
	}
}

/**
 * @brief Result of a ping operation
 */
struct PingResult {
	PingStatus status   = PingStatus::Error;
	uint32_t   rtt_ms   = 0;     // Round-trip time in ms
	uint8_t    ttl      = 0;     // Time to live
	uint32_t   data_len = 0;     // Reply data length

	inline void display() const noexcept {
		if (status == PingStatus::Success) {
			std::print("Ping: status={}, rtt={}ms, ttl={}, bytes={}\n",
				ping_status_name(status), rtt_ms, ttl, data_len);
		} else {
			std::print("Ping: status={}\n", ping_status_name(status));
		}
	}
};

/**
 * @brief Send an ICMP echo request to an IPv4 address
 * @param target Target IP address
 * @param timeout_ms Timeout in milliseconds (default: 1000)
 * @return PingResult with RTT, TTL, and status
 */
inline PingResult ping(const Ip<4>& target, uint32_t timeout_ms = 1000) {
	PingResult result;

#ifdef _WIN32
	// Open ICMP handle
	HANDLE icmp = IcmpCreateFile();
	if (icmp == INVALID_HANDLE_VALUE) {
		result.status = PingStatus::Error;
		return result;
	}

	// Build destination address
	auto b = target.bytes();
	IPAddr dest = (static_cast<uint32_t>(b[3]) << 24)
	            | (static_cast<uint32_t>(b[2]) << 16)
	            | (static_cast<uint32_t>(b[1]) << 8)
	            | static_cast<uint32_t>(b[0]);

	// Send data
	char send_data[] = "etherz-ping";
	DWORD reply_size = sizeof(ICMP_ECHO_REPLY) + sizeof(send_data) + 8;
	std::vector<uint8_t> reply_buf(reply_size);

	DWORD ret = IcmpSendEcho(
		icmp,
		dest,
		send_data,
		sizeof(send_data),
		nullptr,  // No IP options
		reply_buf.data(),
		reply_size,
		timeout_ms
	);

	if (ret > 0) {
		auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(reply_buf.data());
		if (reply->Status == IP_SUCCESS) {
			result.status   = PingStatus::Success;
			result.rtt_ms   = reply->RoundTripTime;
			result.ttl      = static_cast<uint8_t>(reply->Options.Ttl);
			result.data_len = reply->DataSize;
		} else if (reply->Status == IP_REQ_TIMED_OUT) {
			result.status = PingStatus::Timeout;
		} else {
			result.status = PingStatus::Unreachable;
		}
	} else {
		DWORD err = GetLastError();
		if (err == IP_REQ_TIMED_OUT) {
			result.status = PingStatus::Timeout;
		} else {
			result.status = PingStatus::Error;
		}
	}

	IcmpCloseHandle(icmp);

#else
	// POSIX stub — raw ICMP requires elevated privileges
	// Return error by default
	(void)target;
	(void)timeout_ms;
	result.status = PingStatus::Error;
#endif

	return result;
}

} // namespace net
} // namespace etherz

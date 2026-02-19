/**
 * @file error.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Error types for networking operations
 * @version 0.1.0
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace etherz {
namespace core {

/**
 * @brief Error codes for networking operations
 */
enum class Error : uint8_t {
	None = 0,
	InvalidAddress,
	InvalidPort,
	SocketCreationFailed,
	BindFailed,
	ListenFailed,
	AcceptFailed,
	ConnectFailed,
	ConnectionRefused,
	ConnectionReset,
	SendFailed,
	ReceiveFailed,
	Timeout,
	AddressInUse,
	AddressNotAvailable,
	NetworkUnreachable,
	HostUnreachable,
	AlreadyConnected,
	NotConnected,
	SocketClosed,
	Unknown
};

/**
 * @brief Convert error code to human-readable string
 */
inline constexpr std::string_view error_message(Error err) noexcept {
	switch (err) {
		case Error::None:                return "No error";
		case Error::InvalidAddress:      return "Invalid address";
		case Error::InvalidPort:         return "Invalid port";
		case Error::SocketCreationFailed:return "Socket creation failed";
		case Error::BindFailed:          return "Bind failed";
		case Error::ListenFailed:        return "Listen failed";
		case Error::AcceptFailed:        return "Accept failed";
		case Error::ConnectFailed:       return "Connect failed";
		case Error::ConnectionRefused:   return "Connection refused";
		case Error::ConnectionReset:     return "Connection reset";
		case Error::SendFailed:          return "Send failed";
		case Error::ReceiveFailed:       return "Receive failed";
		case Error::Timeout:             return "Operation timed out";
		case Error::AddressInUse:        return "Address already in use";
		case Error::AddressNotAvailable: return "Address not available";
		case Error::NetworkUnreachable:  return "Network unreachable";
		case Error::HostUnreachable:     return "Host unreachable";
		case Error::AlreadyConnected:    return "Already connected";
		case Error::NotConnected:        return "Not connected";
		case Error::SocketClosed:        return "Socket closed";
		case Error::Unknown:             return "Unknown error";
	}
	return "Unknown error";
}

/**
 * @brief Check if an error represents success
 */
inline constexpr bool is_ok(Error err) noexcept {
	return err == Error::None;
}

/**
 * @brief Check if an error represents failure
 */
inline constexpr bool is_error(Error err) noexcept {
	return err != Error::None;
}

} // namespace core
} // namespace etherz

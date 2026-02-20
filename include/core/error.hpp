/**
 * @file error.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Error types for networking operations
 * @version 1.0.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string_view>

// Platform-specific error code headers
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
#else
	#include <cerrno>
#endif

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
	ShutdownFailed,
	OptionFailed,
	WouldBlock,
	HandshakeFailed,
	CertificateError,
	Unknown
};

/**
 * @brief Shutdown mode for socket half-close operations
 */
enum class ShutdownMode : uint8_t {
	Read  = 0,  // Disallow further receives
	Write = 1,  // Disallow further sends
	Both  = 2   // Disallow both
};

/**
 * @brief Convert ShutdownMode to platform-specific constant
 */
inline int to_native(ShutdownMode mode) noexcept {
#ifdef _WIN32
	switch (mode) {
		case ShutdownMode::Read:  return SD_RECEIVE;
		case ShutdownMode::Write: return SD_SEND;
		case ShutdownMode::Both:  return SD_BOTH;
	}
	return SD_BOTH;
#else
	switch (mode) {
		case ShutdownMode::Read:  return SHUT_RD;
		case ShutdownMode::Write: return SHUT_WR;
		case ShutdownMode::Both:  return SHUT_RDWR;
	}
	return SHUT_RDWR;
#endif
}

/**
 * @brief Map platform error code (WSAGetLastError / errno) to Error
 */
inline Error from_platform_error(int code) noexcept {
#ifdef _WIN32
	switch (code) {
		case 0:                    return Error::None;
		case WSAECONNREFUSED:      return Error::ConnectionRefused;
		case WSAECONNRESET:        return Error::ConnectionReset;
		case WSAETIMEDOUT:         return Error::Timeout;
		case WSAEADDRINUSE:        return Error::AddressInUse;
		case WSAEADDRNOTAVAIL:     return Error::AddressNotAvailable;
		case WSAENETUNREACH:       return Error::NetworkUnreachable;
		case WSAEHOSTUNREACH:      return Error::HostUnreachable;
		case WSAEISCONN:           return Error::AlreadyConnected;
		case WSAENOTCONN:          return Error::NotConnected;
		case WSAEWOULDBLOCK:       return Error::WouldBlock;
		case WSAEINPROGRESS:       return Error::WouldBlock;
		default:                   return Error::Unknown;
	}
#else
	switch (code) {
		case 0:                    return Error::None;
		case ECONNREFUSED:         return Error::ConnectionRefused;
		case ECONNRESET:           return Error::ConnectionReset;
		case ETIMEDOUT:            return Error::Timeout;
		case EADDRINUSE:           return Error::AddressInUse;
		case EADDRNOTAVAIL:        return Error::AddressNotAvailable;
		case ENETUNREACH:          return Error::NetworkUnreachable;
		case EHOSTUNREACH:         return Error::HostUnreachable;
		case EISCONN:              return Error::AlreadyConnected;
		case ENOTCONN:             return Error::NotConnected;
		case EWOULDBLOCK:          return Error::WouldBlock;
		case EINPROGRESS:          return Error::WouldBlock;
		default:                   return Error::Unknown;
	}
#endif
}

/**
 * @brief Get the current platform error and map to Error
 */
inline Error last_platform_error() noexcept {
#ifdef _WIN32
	return from_platform_error(WSAGetLastError());
#else
	return from_platform_error(errno);
#endif
}

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
		case Error::ShutdownFailed:      return "Shutdown failed";
		case Error::OptionFailed:        return "Socket option failed";
		case Error::WouldBlock:          return "Operation would block";
		case Error::HandshakeFailed:     return "TLS handshake failed";
		case Error::CertificateError:    return "Certificate error";
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

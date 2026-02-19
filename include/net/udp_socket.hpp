/**
 * @file udp_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Platform-aware UDP Socket wrapper using RAII
 * @version 0.2.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <print>
#include <type_traits>

#include "internet_protocol.hpp"
#include "socket_address.hpp"
#include "../core/error.hpp"

// Platform-specific includes
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#include <fcntl.h>
	#include <cerrno>
#endif

// Use socket impl helpers from socket.hpp
#include "socket.hpp"

namespace etherz {
namespace net {

/**
 * @brief UDP Socket wrapper with RAII lifecycle.
 * 
 * @tparam T The IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class UdpSocket {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>, "Invalid IP version.");
};

// ═══════════════════════════════════════════════
//  UdpSocket<Ip<4>> — UDP IPv4
// ═══════════════════════════════════════════════

template <>
class UdpSocket<Ip<4>> {
public:
	using protocol_type = Ip<4>;
	using address_type = SocketAddress<Ip<4>>;

	/**
	 * @brief Result of a recv_from operation
	 */
	struct RecvResult {
		int bytes;
		address_type sender;
		core::Error error;
	};

	UdpSocket() noexcept = default;
	~UdpSocket() noexcept { close(); }

	// Non-copyable, movable
	UdpSocket(const UdpSocket&) = delete;
	UdpSocket& operator=(const UdpSocket&) = delete;

	UdpSocket(UdpSocket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	UdpSocket& operator=(UdpSocket&& other) noexcept {
		if (this != &other) {
			close();
			fd_ = other.fd_;
			other.fd_ = impl::invalid_socket;
		}
		return *this;
	}

	// ─── Lifecycle ──────────────────────

	core::Error create() noexcept {
		impl::ensure_wsa();
		fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (fd_ == impl::invalid_socket) return core::last_platform_error();
		return core::Error::None;
	}

	core::Error bind(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(addr.port());
		sa.sin_addr.s_addr = addr.address().to_network();
		if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	// ─── Data Transfer ──────────────────

	/**
	 * @brief Send data to a specific address
	 * @return Number of bytes sent, or -1 on error
	 */
	int send_to(std::span<const uint8_t> data, const address_type& dest) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		struct sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(dest.port());
		sa.sin_addr.s_addr = dest.address().to_network();
		return static_cast<int>(::sendto(fd_, reinterpret_cast<const char*>(data.data()),
			static_cast<int>(data.size()), 0,
			reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)));
	}

	/**
	 * @brief Receive data and sender address
	 */
	RecvResult recv_from(std::span<uint8_t> buffer) noexcept {
		RecvResult result{};
		if (fd_ == impl::invalid_socket) {
			result.bytes = -1;
			result.error = core::Error::SocketClosed;
			return result;
		}

		struct sockaddr_in sender_addr{};
#ifdef _WIN32
		int sender_len = sizeof(sender_addr);
#else
		socklen_t sender_len = sizeof(sender_addr);
#endif

		result.bytes = static_cast<int>(::recvfrom(fd_, reinterpret_cast<char*>(buffer.data()),
			static_cast<int>(buffer.size()), 0,
			reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_len));

		if (result.bytes < 0) {
			result.error = core::last_platform_error();
			return result;
		}

		uint32_t net_addr = ntohl(sender_addr.sin_addr.s_addr);
		result.sender = address_type(protocol_type(net_addr), ntohs(sender_addr.sin_port));
		result.error = core::Error::None;
		return result;
	}

	void close() noexcept {
		if (fd_ != impl::invalid_socket) {
			impl::close_socket(fd_);
			fd_ = impl::invalid_socket;
		}
	}

	// ─── Shutdown ───────────────────────

	core::Error shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		if (::shutdown(fd_, core::to_native(mode)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	// ─── Socket Options ─────────────────

	core::Error set_reuse_addr(bool enable = true) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = enable ? 1 : 0;
		return impl::set_sock_opt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	}

	core::Error set_nonblocking(bool enable = true) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		return impl::set_nonblocking_impl(fd_, enable);
	}

	core::Error set_timeout(uint32_t ms) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
#ifdef _WIN32
		DWORD timeout = static_cast<DWORD>(ms);
		auto err = impl::set_sock_opt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
		if (core::is_error(err)) return err;
		return impl::set_sock_opt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#else
		struct timeval tv{};
		tv.tv_sec = static_cast<time_t>(ms / 1000);
		tv.tv_usec = static_cast<suseconds_t>((ms % 1000) * 1000);
		auto err = impl::set_sock_opt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if (core::is_error(err)) return err;
		return impl::set_sock_opt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
	}

	// ─── Queries ────────────────────────

	bool is_open() const noexcept { return fd_ != impl::invalid_socket; }
	impl::socket_t native_handle() const noexcept { return fd_; }

private:
	impl::socket_t fd_ = impl::invalid_socket;
};

} // namespace net
} // namespace etherz

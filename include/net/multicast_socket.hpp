/**
 * @file multicast_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Multicast UDP socket wrapper
 * @version 1.1.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <print>
#include <type_traits>
#include <string_view>

#include "internet_protocol.hpp"
#include "socket_address.hpp"
#include "udp_socket.hpp"
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

#include "socket.hpp"

namespace etherz {
namespace net {

/**
 * @brief Multicast UDP socket for group communication
 * 
 * Extends UdpSocket with multicast-specific operations:
 * join/leave multicast groups, set TTL, and configure loopback.
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class MulticastSocket {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>,
		"Invalid IP version.");
};

// ═══════════════════════════════════════════════
//  MulticastSocket<Ip<4>> — IPv4 Multicast
// ═══════════════════════════════════════════════

template <>
class MulticastSocket<Ip<4>> {
public:
	using protocol_type = Ip<4>;
	using address_type = SocketAddress<Ip<4>>;

	struct RecvResult {
		int bytes;
		address_type sender;
		core::Error error = core::Error::None;
	};

	MulticastSocket() noexcept = default;
	~MulticastSocket() noexcept { close(); }

	// Non-copyable, movable
	MulticastSocket(const MulticastSocket&) = delete;
	MulticastSocket& operator=(const MulticastSocket&) = delete;

	MulticastSocket(MulticastSocket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	MulticastSocket& operator=(MulticastSocket&& other) noexcept {
		if (this != &other) {
			close();
			fd_ = other.fd_;
			other.fd_ = impl::invalid_socket;
		}
		return *this;
	}

	// ─── Lifecycle ──────────────────────

	/**
	 * @brief Create the multicast UDP socket
	 */
	core::Error create() noexcept {
		impl::ensure_wsa();
		fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (fd_ == impl::invalid_socket) return core::last_platform_error();
		return core::Error::None;
	}

	/**
	 * @brief Bind the socket to a local address
	 */
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

	// ─── Multicast Operations ───────────

	/**
	 * @brief Join a multicast group
	 * @param group Multicast group address (224.0.0.0 – 239.255.255.255)
	 * @param iface Local interface to join on (0.0.0.0 = default)
	 */
	core::Error join_group(const Ip<4>& group, const Ip<4>& iface = Ip<4>(0, 0, 0, 0)) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct ip_mreq mreq{};
		mreq.imr_multiaddr.s_addr = group.to_network();
		mreq.imr_interface.s_addr = iface.to_network();

		if (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == impl::socket_error) {
			return core::Error::MulticastError;
		}
		return core::Error::None;
	}

	/**
	 * @brief Leave a multicast group
	 * @param group Multicast group address
	 * @param iface Local interface (0.0.0.0 = default)
	 */
	core::Error leave_group(const Ip<4>& group, const Ip<4>& iface = Ip<4>(0, 0, 0, 0)) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct ip_mreq mreq{};
		mreq.imr_multiaddr.s_addr = group.to_network();
		mreq.imr_interface.s_addr = iface.to_network();

		if (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
			reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == impl::socket_error) {
			return core::Error::MulticastError;
		}
		return core::Error::None;
	}

	/**
	 * @brief Set multicast TTL (Time To Live / hop count)
	 * @param ttl Hop limit (1 = local subnet, 255 = unrestricted)
	 */
	core::Error set_ttl(uint8_t ttl) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = static_cast<int>(ttl);
		return impl::set_sock_opt(fd_, IPPROTO_IP, IP_MULTICAST_TTL, &val, sizeof(val));
	}

	/**
	 * @brief Enable/disable multicast loopback
	 * @param enable If true, sender also receives its own multicast messages
	 */
	core::Error set_loopback(bool enable) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = enable ? 1 : 0;
		return impl::set_sock_opt(fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &val, sizeof(val));
	}

	/**
	 * @brief Set the outgoing multicast interface
	 * @param iface Local interface IP
	 */
	core::Error set_interface(const Ip<4>& iface) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct in_addr addr{};
		addr.s_addr = iface.to_network();
		return impl::set_sock_opt(fd_, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr));
	}

	// ─── Data Transfer ──────────────────

	/**
	 * @brief Send data to a multicast group address
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

// ═══════════════════════════════════════════════
//  MulticastSocket<Ip<6>> — IPv6 Multicast
// ═══════════════════════════════════════════════

template <>
class MulticastSocket<Ip<6>> {
public:
	using protocol_type = Ip<6>;
	using address_type = SocketAddress<Ip<6>>;

	struct RecvResult {
		int bytes;
		address_type sender;
		core::Error error = core::Error::None;
	};

	MulticastSocket() noexcept = default;
	~MulticastSocket() noexcept { close(); }

	// Non-copyable, movable
	MulticastSocket(const MulticastSocket&) = delete;
	MulticastSocket& operator=(const MulticastSocket&) = delete;

	MulticastSocket(MulticastSocket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	MulticastSocket& operator=(MulticastSocket&& other) noexcept {
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
		fd_ = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (fd_ == impl::invalid_socket) return core::last_platform_error();
		return core::Error::None;
	}

	core::Error bind(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_in6 sa{};
		sa.sin6_family = AF_INET6;
		sa.sin6_port = htons(addr.port());
		fill_in6_addr(sa.sin6_addr, addr.address());
		if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	// ─── Multicast Operations ───────────

	/**
	 * @brief Join an IPv6 multicast group
	 * @param group Multicast group address (ff00::/8)
	 * @param iface_index Interface index (0 = default)
	 */
	core::Error join_group(const Ip<6>& group, uint32_t iface_index = 0) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct ipv6_mreq mreq{};
		fill_in6_addr(mreq.ipv6mr_multiaddr, group);
		mreq.ipv6mr_interface = iface_index;

		if (::setsockopt(fd_, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == impl::socket_error) {
			return core::Error::MulticastError;
		}
		return core::Error::None;
	}

	/**
	 * @brief Leave an IPv6 multicast group
	 */
	core::Error leave_group(const Ip<6>& group, uint32_t iface_index = 0) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct ipv6_mreq mreq{};
		fill_in6_addr(mreq.ipv6mr_multiaddr, group);
		mreq.ipv6mr_interface = iface_index;

		if (::setsockopt(fd_, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
			reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == impl::socket_error) {
			return core::Error::MulticastError;
		}
		return core::Error::None;
	}

	/**
	 * @brief Set multicast hop limit for IPv6
	 * @param hops Hop limit (1 = link-local, 255 = unrestricted)
	 */
	core::Error set_ttl(uint8_t hops) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = static_cast<int>(hops);
		return impl::set_sock_opt(fd_, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &val, sizeof(val));
	}

	/**
	 * @brief Enable/disable multicast loopback for IPv6
	 */
	core::Error set_loopback(bool enable) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = enable ? 1 : 0;
		return impl::set_sock_opt(fd_, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &val, sizeof(val));
	}

	// ─── Data Transfer ──────────────────

	int send_to(std::span<const uint8_t> data, const address_type& dest) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		struct sockaddr_in6 sa{};
		sa.sin6_family = AF_INET6;
		sa.sin6_port = htons(dest.port());
		fill_in6_addr(sa.sin6_addr, dest.address());
		return static_cast<int>(::sendto(fd_, reinterpret_cast<const char*>(data.data()),
			static_cast<int>(data.size()), 0,
			reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)));
	}

	RecvResult recv_from(std::span<uint8_t> buffer) noexcept {
		RecvResult result{};
		if (fd_ == impl::invalid_socket) {
			result.bytes = -1;
			result.error = core::Error::SocketClosed;
			return result;
		}

		struct sockaddr_in6 sender_addr{};
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

		result.sender = address_type(extract_ip6(sender_addr.sin6_addr), ntohs(sender_addr.sin6_port));
		result.error = core::Error::None;
		return result;
	}

	void close() noexcept {
		if (fd_ != impl::invalid_socket) {
			impl::close_socket(fd_);
			fd_ = impl::invalid_socket;
		}
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

	// ─── Queries ────────────────────────

	bool is_open() const noexcept { return fd_ != impl::invalid_socket; }
	impl::socket_t native_handle() const noexcept { return fd_; }

private:
	impl::socket_t fd_ = impl::invalid_socket;

	static void fill_in6_addr(struct in6_addr& dst, const Ip<6>& src) noexcept {
		const auto& groups = src.bytes();
		for (size_t i = 0; i < 8; ++i) {
			dst.s6_addr[i * 2]     = static_cast<uint8_t>((groups[i] >> 8) & 0xFF);
			dst.s6_addr[i * 2 + 1] = static_cast<uint8_t>(groups[i] & 0xFF);
		}
	}

	static Ip<6> extract_ip6(const struct in6_addr& src) noexcept {
		return Ip<6>(
			static_cast<uint16_t>((src.s6_addr[0]  << 8) | src.s6_addr[1]),
			static_cast<uint16_t>((src.s6_addr[2]  << 8) | src.s6_addr[3]),
			static_cast<uint16_t>((src.s6_addr[4]  << 8) | src.s6_addr[5]),
			static_cast<uint16_t>((src.s6_addr[6]  << 8) | src.s6_addr[7]),
			static_cast<uint16_t>((src.s6_addr[8]  << 8) | src.s6_addr[9]),
			static_cast<uint16_t>((src.s6_addr[10] << 8) | src.s6_addr[11]),
			static_cast<uint16_t>((src.s6_addr[12] << 8) | src.s6_addr[13]),
			static_cast<uint16_t>((src.s6_addr[14] << 8) | src.s6_addr[15])
		);
	}
};

} // namespace net
} // namespace etherz

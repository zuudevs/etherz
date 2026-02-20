/**
 * @file socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Platform-aware TCP Socket wrapper using RAII
 * @version 1.0.0
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

namespace etherz {
namespace net {

namespace impl {

#ifdef _WIN32
	using socket_t = SOCKET;
	constexpr socket_t invalid_socket = INVALID_SOCKET;
	constexpr int socket_error = SOCKET_ERROR;

	/**
	 * @brief Initialize WinSock (RAII guard)
	 */
	struct WsaGuard {
		bool initialized = false;

		WsaGuard() noexcept {
			WSADATA wsa_data;
			initialized = (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0);
		}

		~WsaGuard() noexcept {
			if (initialized) WSACleanup();
		}

		WsaGuard(const WsaGuard&) = delete;
		WsaGuard& operator=(const WsaGuard&) = delete;
	};

	inline void ensure_wsa() noexcept {
		static WsaGuard guard;
		(void)guard;
	}

	inline int close_socket(socket_t s) noexcept { return closesocket(s); }
#else
	using socket_t = int;
	constexpr socket_t invalid_socket = -1;
	constexpr int socket_error = -1;

	inline void ensure_wsa() noexcept {} // No-op on POSIX
	inline int close_socket(socket_t s) noexcept { return close(s); }
#endif

	/**
	 * @brief Set socket option helper
	 */
	inline core::Error set_sock_opt(socket_t fd, int level, int optname, const void* val, int len) noexcept {
		if (::setsockopt(fd, level, optname, reinterpret_cast<const char*>(val), len) == socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	/**
	 * @brief Set socket to non-blocking mode
	 */
	inline core::Error set_nonblocking_impl(socket_t fd, bool enable) noexcept {
#ifdef _WIN32
		u_long mode = enable ? 1 : 0;
		if (ioctlsocket(fd, FIONBIO, &mode) == socket_error)
			return core::last_platform_error();
#else
		int flags = fcntl(fd, F_GETFL, 0);
		if (flags == -1) return core::last_platform_error();
		flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
		if (fcntl(fd, F_SETFL, flags) == -1) return core::last_platform_error();
#endif
		return core::Error::None;
	}

} // namespace impl

/**
 * @brief TCP Socket wrapper with RAII lifecycle.
 * 
 * @tparam T The IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class Socket {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>, "Invalid IP version.");
};

// ═══════════════════════════════════════════════
//  Socket<Ip<4>> — TCP IPv4
// ═══════════════════════════════════════════════

template <>
class Socket<Ip<4>> {
public:
	using protocol_type = Ip<4>;
	using address_type = SocketAddress<Ip<4>>;

	Socket() noexcept = default;
	~Socket() noexcept { close(); }

	// Non-copyable, movable
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	Socket(Socket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	Socket& operator=(Socket&& other) noexcept {
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
		fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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

	core::Error listen(int backlog = SOMAXCONN) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		if (::listen(fd_, backlog) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	struct AcceptResult {
		impl::socket_t client_fd = impl::invalid_socket;
		address_type address;
		core::Error error = core::Error::None;

		/**
		 * @brief Take ownership of the client socket.
		 * After calling this, client_fd is invalidated.
		 */
		Socket take_client() noexcept {
			Socket s;
			s.fd_ = client_fd;
			client_fd = impl::invalid_socket;
			return s;
		}
	};

	AcceptResult accept() noexcept {
		AcceptResult result;
		if (fd_ == impl::invalid_socket) {
			result.error = core::Error::SocketClosed;
			return result;
		}

		struct sockaddr_in client_addr{};
#ifdef _WIN32
		int client_len = sizeof(client_addr);
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
#else
		socklen_t client_len = sizeof(client_addr);
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
#endif

		if (client_fd == impl::invalid_socket) {
			result.error = core::last_platform_error();
			return result;
		}

		result.client_fd = client_fd;
		uint32_t net_addr = ntohl(client_addr.sin_addr.s_addr);
		result.address = address_type(protocol_type(net_addr), ntohs(client_addr.sin_port));
		result.error = core::Error::None;
		return result;
	}

	core::Error connect(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(addr.port());
		sa.sin_addr.s_addr = addr.address().to_network();
		if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	int send(std::span<const uint8_t> data) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::send(fd_, reinterpret_cast<const char*>(data.data()),
			static_cast<int>(data.size()), 0));
	}

	int recv(std::span<uint8_t> buffer) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::recv(fd_, reinterpret_cast<char*>(buffer.data()),
			static_cast<int>(buffer.size()), 0));
	}

	void close() noexcept {
		if (fd_ != impl::invalid_socket) {
			impl::close_socket(fd_);
			fd_ = impl::invalid_socket;
		}
	}

	// ─── Shutdown ───────────────────────

	/**
	 * @brief Graceful half-close of the socket
	 */
	core::Error shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		if (::shutdown(fd_, core::to_native(mode)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	// ─── Socket Options ─────────────────

	/**
	 * @brief Enable/disable SO_REUSEADDR
	 */
	core::Error set_reuse_addr(bool enable = true) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		int val = enable ? 1 : 0;
		return impl::set_sock_opt(fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
	}

	/**
	 * @brief Enable/disable non-blocking mode
	 */
	core::Error set_nonblocking(bool enable = true) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		return impl::set_nonblocking_impl(fd_, enable);
	}

	/**
	 * @brief Set send and receive timeout in milliseconds
	 */
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
	friend class Socket;
};

// ═══════════════════════════════════════════════
//  Socket<Ip<6>> — TCP IPv6
// ═══════════════════════════════════════════════

template <>
class Socket<Ip<6>> {
public:
	using protocol_type = Ip<6>;
	using address_type = SocketAddress<Ip<6>>;

	Socket() noexcept = default;
	~Socket() noexcept { close(); }

	// Non-copyable, movable
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	Socket(Socket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	Socket& operator=(Socket&& other) noexcept {
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
		fd_ = ::socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
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

	core::Error listen(int backlog = SOMAXCONN) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		if (::listen(fd_, backlog) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	struct AcceptResult {
		impl::socket_t client_fd = impl::invalid_socket;
		address_type address;
		core::Error error = core::Error::None;

		Socket take_client() noexcept {
			Socket s;
			s.fd_ = client_fd;
			client_fd = impl::invalid_socket;
			return s;
		}
	};

	AcceptResult accept() noexcept {
		AcceptResult result;
		if (fd_ == impl::invalid_socket) {
			result.error = core::Error::SocketClosed;
			return result;
		}

		struct sockaddr_in6 client_addr{};
#ifdef _WIN32
		int client_len = sizeof(client_addr);
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
#else
		socklen_t client_len = sizeof(client_addr);
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
#endif

		if (client_fd == impl::invalid_socket) {
			result.error = core::last_platform_error();
			return result;
		}

		result.client_fd = client_fd;
		result.address = address_type(extract_ip6(client_addr.sin6_addr), ntohs(client_addr.sin6_port));
		result.error = core::Error::None;
		return result;
	}

	core::Error connect(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_in6 sa{};
		sa.sin6_family = AF_INET6;
		sa.sin6_port = htons(addr.port());
		fill_in6_addr(sa.sin6_addr, addr.address());
		if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	int send(std::span<const uint8_t> data) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::send(fd_, reinterpret_cast<const char*>(data.data()),
			static_cast<int>(data.size()), 0));
	}

	int recv(std::span<uint8_t> buffer) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::recv(fd_, reinterpret_cast<char*>(buffer.data()),
			static_cast<int>(buffer.size()), 0));
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
	friend class Socket;

	/**
	 * @brief Copy Ip<6> groups into sockaddr_in6.sin6_addr
	 */
	static void fill_in6_addr(struct in6_addr& dst, const Ip<6>& src) noexcept {
		const auto& groups = src.bytes();
		for (size_t i = 0; i < 8; ++i) {
			dst.s6_addr[i * 2]     = static_cast<uint8_t>((groups[i] >> 8) & 0xFF);
			dst.s6_addr[i * 2 + 1] = static_cast<uint8_t>(groups[i] & 0xFF);
		}
	}

	/**
	 * @brief Extract Ip<6> from sockaddr_in6.sin6_addr
	 */
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

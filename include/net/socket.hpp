/**
 * @file socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Platform-aware Socket wrapper using RAII
 * @version 0.1.0
 * @date 2026-02-18
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

	inline int close_socket(socket_t s) noexcept { return closesocket(s); }
	inline int last_error() noexcept { return WSAGetLastError(); }
#else
	using socket_t = int;
	constexpr socket_t invalid_socket = -1;
	constexpr int socket_error = -1;

	inline int close_socket(socket_t s) noexcept { return close(s); }
	inline int last_error() noexcept { return errno; }
#endif

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

/**
 * @brief TCP Socket specialization for IPv4
 */
template <>
class Socket<Ip<4>> {
public:
	using protocol_type = Ip<4>;
	using address_type = SocketAddress<Ip<4>>;

	Socket() noexcept = default;

	~Socket() noexcept { close(); }

	// Non-copyable
	Socket(const Socket&) = delete;
	Socket& operator=(const Socket&) = delete;

	// Movable
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

	/**
	 * @brief Create the underlying socket
	 */
	core::Error create() noexcept {
#ifdef _WIN32
		static impl::WsaGuard wsa_guard;
		if (!wsa_guard.initialized) return core::Error::SocketCreationFailed;
#endif
		fd_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (fd_ == impl::invalid_socket) return core::Error::SocketCreationFailed;
		return core::Error::None;
	}

	/**
	 * @brief Bind socket to address
	 */
	core::Error bind(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(addr.port());
		sa.sin_addr.s_addr = addr.address().to_network();

		if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::Error::BindFailed;
		
		return core::Error::None;
	}

	/**
	 * @brief Listen for incoming connections
	 */
	core::Error listen(int backlog = SOMAXCONN) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		if (::listen(fd_, backlog) == impl::socket_error)
			return core::Error::ListenFailed;
		return core::Error::None;
	}

	/**
	 * @brief Accept an incoming connection
	 * @return Pair of new Socket and the client address
	 */
	struct AcceptResult {
		Socket client;
		address_type address;
		core::Error error;
	};

	AcceptResult accept() noexcept {
		AcceptResult result;
		if (fd_ == impl::invalid_socket) {
			result.error = core::Error::SocketClosed;
			return result;
		}

		struct sockaddr_in client_addr{};
		int client_len = sizeof(client_addr);
#ifdef _WIN32
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
#else
		socklen_t len = static_cast<socklen_t>(client_len);
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &len);
#endif

		if (client_fd == impl::invalid_socket) {
			result.error = core::Error::AcceptFailed;
			return result;
		}

		result.client.fd_ = client_fd;
		uint32_t net_addr = ntohl(client_addr.sin_addr.s_addr);
		result.address = address_type(
			protocol_type(net_addr),
			ntohs(client_addr.sin_port)
		);
		result.error = core::Error::None;
		return result;
	}

	/**
	 * @brief Connect to a remote address
	 */
	core::Error connect(const address_type& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;

		struct sockaddr_in sa{};
		sa.sin_family = AF_INET;
		sa.sin_port = htons(addr.port());
		sa.sin_addr.s_addr = addr.address().to_network();

		if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::Error::ConnectFailed;
		
		return core::Error::None;
	}

	/**
	 * @brief Send data through the socket
	 * @return Number of bytes sent, or -1 on error
	 */
	int send(std::span<const uint8_t> data) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::send(fd_, reinterpret_cast<const char*>(data.data()), 
			static_cast<int>(data.size()), 0));
	}

	/**
	 * @brief Receive data from the socket
	 * @return Number of bytes received, or -1 on error
	 */
	int recv(std::span<uint8_t> buffer) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		return static_cast<int>(::recv(fd_, reinterpret_cast<char*>(buffer.data()), 
			static_cast<int>(buffer.size()), 0));
	}

	/**
	 * @brief Close the socket
	 */
	void close() noexcept {
		if (fd_ != impl::invalid_socket) {
			impl::close_socket(fd_);
			fd_ = impl::invalid_socket;
		}
	}

	/**
	 * @brief Check if the socket is open/valid
	 */
	bool is_open() const noexcept { return fd_ != impl::invalid_socket; }

	/**
	 * @brief Get the raw socket file descriptor
	 */
	impl::socket_t native_handle() const noexcept { return fd_; }

private:
	impl::socket_t fd_ = impl::invalid_socket;

	// Allow accept() to set fd_ on the returned socket
	friend class Socket;
};

} // namespace net
} // namespace etherz

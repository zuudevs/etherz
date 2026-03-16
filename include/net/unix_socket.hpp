/**
 * @file unix_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Unix domain socket (AF_UNIX) wrapper
 * @version 1.5.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <expected>

#include "../core/error.hpp"

// Unix domain sockets are POSIX-only (also available on Windows 10+)
#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <afunix.h>
	#pragma comment(lib, "ws2_32.lib")
	#include "socket.hpp"
#else
	#include <sys/socket.h>
	#include <sys/un.h>
	#include <unistd.h>
	#include <cstring>
	#include "socket.hpp"
#endif

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Unix Socket Address
// ═══════════════════════════════════════════════

/**
 * @brief Path-based address for Unix domain sockets
 */
class UnixSocketAddress {
public:
	UnixSocketAddress() noexcept = default;

	explicit UnixSocketAddress(std::string_view path)
		: path_(path) {}

	const std::string& path() const noexcept { return path_; }
	std::string display() const { return path_; }

	bool operator==(const UnixSocketAddress&) const = default;
	auto operator<=>(const UnixSocketAddress&) const = default;

private:
	std::string path_;
};

// ═══════════════════════════════════════════════
//  Unix Stream Socket (SOCK_STREAM)
// ═══════════════════════════════════════════════

/**
 * @brief RAII Unix domain stream socket (AF_UNIX, SOCK_STREAM)
 * 
 * Provides IPC communication over Unix domain sockets using
 * filesystem paths as addresses.
 * 
 * Note: On Windows, requires Windows 10 build 17063 or later.
 */
class UnixSocket {
public:
	UnixSocket() noexcept = default;
	~UnixSocket() noexcept { close(); }

	// Non-copyable, movable
	UnixSocket(const UnixSocket&) = delete;
	UnixSocket& operator=(const UnixSocket&) = delete;

	UnixSocket(UnixSocket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	UnixSocket& operator=(UnixSocket&& other) noexcept {
		if (this != &other) {
			close();
			fd_ = other.fd_;
			other.fd_ = impl::invalid_socket;
		}
		return *this;
	}

	// ─── Lifecycle ──────────────────────

	core::Error create() noexcept {
#ifdef _WIN32
		impl::ensure_wsa();
#endif
		fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
		if (fd_ == impl::invalid_socket) return core::last_platform_error();
		return core::Error::None;
	}

	core::Error bind(const UnixSocketAddress& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_un sa{};
		sa.sun_family = AF_UNIX;
		auto& path = addr.path();
		if (path.size() >= sizeof(sa.sun_path)) return core::Error::InvalidAddress;
		std::memcpy(sa.sun_path, path.c_str(), path.size() + 1);

#ifndef _WIN32
		// Remove existing socket file
		::unlink(path.c_str());
#endif

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

	std::expected<UnixSocket, core::Error> accept() noexcept {
		if (fd_ == impl::invalid_socket) {
			return std::unexpected(core::Error::SocketClosed);
		}

		struct sockaddr_un client_addr{};
#ifdef _WIN32
		int client_len = sizeof(client_addr);
#else
		socklen_t client_len = sizeof(client_addr);
#endif
		auto client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
		if (client_fd == impl::invalid_socket) {
			return std::unexpected(core::last_platform_error());
		}

		UnixSocket client;
		client.fd_ = client_fd;
		return client;
	}

	core::Error connect(const UnixSocketAddress& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_un sa{};
		sa.sun_family = AF_UNIX;
		auto& path = addr.path();
		if (path.size() >= sizeof(sa.sun_path)) return core::Error::InvalidAddress;
		std::memcpy(sa.sun_path, path.c_str(), path.size() + 1);

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

	// ─── Socket Options ─────────────────

	core::Error set_nonblocking(bool enable = true) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		return impl::set_nonblocking_impl(fd_, enable);
	}

	// ─── Queries ────────────────────────

	bool is_open() const noexcept { return fd_ != impl::invalid_socket; }
	impl::socket_t native_handle() const noexcept { return fd_; }

private:
	impl::socket_t fd_ = impl::invalid_socket;
};

// ═══════════════════════════════════════════════
//  Unix Datagram Socket (SOCK_DGRAM)
// ═══════════════════════════════════════════════

/**
 * @brief RAII Unix domain datagram socket (AF_UNIX, SOCK_DGRAM)
 */
class UnixDatagramSocket {
public:
	struct RecvResult {
		int bytes;
		UnixSocketAddress sender;
		core::Error error = core::Error::None;
	};

	UnixDatagramSocket() noexcept = default;
	~UnixDatagramSocket() noexcept { close(); }

	// Non-copyable, movable
	UnixDatagramSocket(const UnixDatagramSocket&) = delete;
	UnixDatagramSocket& operator=(const UnixDatagramSocket&) = delete;

	UnixDatagramSocket(UnixDatagramSocket&& other) noexcept : fd_(other.fd_) {
		other.fd_ = impl::invalid_socket;
	}

	UnixDatagramSocket& operator=(UnixDatagramSocket&& other) noexcept {
		if (this != &other) {
			close();
			fd_ = other.fd_;
			other.fd_ = impl::invalid_socket;
		}
		return *this;
	}

	core::Error create() noexcept {
#ifdef _WIN32
		impl::ensure_wsa();
#endif
		fd_ = ::socket(AF_UNIX, SOCK_DGRAM, 0);
		if (fd_ == impl::invalid_socket) return core::last_platform_error();
		return core::Error::None;
	}

	core::Error bind(const UnixSocketAddress& addr) noexcept {
		if (fd_ == impl::invalid_socket) return core::Error::SocketClosed;
		struct sockaddr_un sa{};
		sa.sun_family = AF_UNIX;
		auto& path = addr.path();
		if (path.size() >= sizeof(sa.sun_path)) return core::Error::InvalidAddress;
		std::memcpy(sa.sun_path, path.c_str(), path.size() + 1);

#ifndef _WIN32
		::unlink(path.c_str());
#endif

		if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == impl::socket_error)
			return core::last_platform_error();
		return core::Error::None;
	}

	int send_to(std::span<const uint8_t> data, const UnixSocketAddress& dest) noexcept {
		if (fd_ == impl::invalid_socket) return -1;
		struct sockaddr_un sa{};
		sa.sun_family = AF_UNIX;
		auto& path = dest.path();
		if (path.size() >= sizeof(sa.sun_path)) return -1;
		std::memcpy(sa.sun_path, path.c_str(), path.size() + 1);

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

		struct sockaddr_un sender_addr{};
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

		result.sender = UnixSocketAddress(sender_addr.sun_path);
		result.error = core::Error::None;
		return result;
	}

	void close() noexcept {
		if (fd_ != impl::invalid_socket) {
			impl::close_socket(fd_);
			fd_ = impl::invalid_socket;
		}
	}

	bool is_open() const noexcept { return fd_ != impl::invalid_socket; }
	impl::socket_t native_handle() const noexcept { return fd_; }

private:
	impl::socket_t fd_ = impl::invalid_socket;
};

} // namespace net
} // namespace etherz

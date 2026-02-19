/**
 * @file async_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Async socket wrapper with callback-based operations
 * @version 0.3.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <functional>
#include <type_traits>
#include <print>

#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../core/error.hpp"
#include "event_loop.hpp"

namespace etherz {
namespace async {

/**
 * @brief Async TCP socket wrapper with callback-based I/O.
 * 
 * Wraps a Socket<T> in non-blocking mode and integrates with EventLoop
 * for event-driven async connect, accept, send, and recv operations.
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class AsyncSocket {
	static_assert(std::is_same_v<T, net::Ip<4>> || std::is_same_v<T, net::Ip<6>>,
		"Invalid IP version.");

public:
	using protocol_type = T;
	using address_type = net::SocketAddress<T>;
	using socket_type = net::Socket<T>;

	// ─── Callback signatures ────────────

	using ConnectCallback = std::function<void(core::Error)>;
	using AcceptCallback  = std::function<void(core::Error, net::impl::socket_t, address_type)>;
	using SendCallback    = std::function<void(core::Error, int bytes_sent)>;
	using RecvCallback    = std::function<void(core::Error, int bytes_received)>;

	AsyncSocket() noexcept = default;

	// Non-copyable, movable
	AsyncSocket(const AsyncSocket&) = delete;
	AsyncSocket& operator=(const AsyncSocket&) = delete;
	AsyncSocket(AsyncSocket&&) noexcept = default;
	AsyncSocket& operator=(AsyncSocket&&) noexcept = default;

	/**
	 * @brief Create the underlying socket and set non-blocking mode
	 */
	core::Error create() noexcept {
		auto err = socket_.create();
		if (core::is_error(err)) return err;
		return socket_.set_nonblocking(true);
	}

	/**
	 * @brief Bind the socket to an address
	 */
	core::Error bind(const address_type& addr) noexcept {
		return socket_.bind(addr);
	}

	/**
	 * @brief Start listening for connections
	 */
	core::Error listen(int backlog = SOMAXCONN) noexcept {
		return socket_.listen(backlog);
	}

	/**
	 * @brief Async connect: registers with the event loop and calls back when connected
	 */
	void async_connect(const address_type& addr, EventLoop& loop, ConnectCallback cb) {
		auto err = socket_.connect(addr);
		if (core::is_ok(err)) {
			// Connected immediately (local connections)
			if (cb) cb(core::Error::None);
			return;
		}
		if (err != core::Error::WouldBlock) {
			// Real error
			if (cb) cb(err);
			return;
		}

		// Connect in progress — wait for WriteReady
		auto fd = socket_.native_handle();
		loop.add(fd, PollEvent::WriteReady, [cb = std::move(cb), &loop, fd]
			(net::impl::socket_t, PollEvent events) {
				loop.remove(fd);
				if (has_event(events, PollEvent::Error)) {
					cb(core::Error::ConnectFailed);
				} else {
					cb(core::Error::None);
				}
			});
	}

	/**
	 * @brief Async accept: registers with the event loop and calls back with new client
	 */
	void async_accept(EventLoop& loop, AcceptCallback cb) {
		auto fd = socket_.native_handle();
		loop.add(fd, PollEvent::ReadReady, [this, cb = std::move(cb), &loop, fd]
			(net::impl::socket_t, PollEvent events) {
				if (has_event(events, PollEvent::Error)) {
					loop.remove(fd);
					cb(core::Error::AcceptFailed, net::impl::invalid_socket, address_type{});
					return;
				}

				auto result = socket_.accept();
				if (core::is_error(result.error)) {
					if (result.error == core::Error::WouldBlock) {
						return; // Spurious wake, keep listening
					}
					loop.remove(fd);
					cb(result.error, net::impl::invalid_socket, address_type{});
					return;
				}

				// Don't remove — keep listening for more connections
				cb(core::Error::None, result.client_fd, result.address);
			});
	}

	/**
	 * @brief Async send: registers with the event loop and calls back with bytes sent
	 */
	void async_send(std::span<const uint8_t> data, EventLoop& loop, SendCallback cb) {
		auto fd = socket_.native_handle();
		loop.add(fd, PollEvent::WriteReady, [this, data, cb = std::move(cb), &loop, fd]
			(net::impl::socket_t, PollEvent events) {
				loop.remove(fd);
				if (has_event(events, PollEvent::Error)) {
					cb(core::Error::SendFailed, -1);
					return;
				}
				int sent = socket_.send(data);
				if (sent < 0) {
					cb(core::last_platform_error(), -1);
				} else {
					cb(core::Error::None, sent);
				}
			});
	}

	/**
	 * @brief Async recv: registers with the event loop and calls back with bytes received
	 */
	void async_recv(std::span<uint8_t> buffer, EventLoop& loop, RecvCallback cb) {
		auto fd = socket_.native_handle();
		loop.add(fd, PollEvent::ReadReady, [this, buffer, cb = std::move(cb), &loop, fd]
			(net::impl::socket_t, PollEvent events) {
				loop.remove(fd);
				if (has_event(events, PollEvent::Error)) {
					cb(core::Error::ReceiveFailed, -1);
					return;
				}
				int received = socket_.recv(buffer);
				if (received < 0) {
					cb(core::last_platform_error(), -1);
				} else {
					cb(core::Error::None, received);
				}
			});
	}

	// ─── Options / State delegators ─────

	core::Error set_reuse_addr(bool enable = true) noexcept { return socket_.set_reuse_addr(enable); }
	core::Error set_timeout(uint32_t ms) noexcept { return socket_.set_timeout(ms); }
	core::Error shutdown(core::ShutdownMode mode = core::ShutdownMode::Both) noexcept {
		return socket_.shutdown(mode);
	}
	void close() noexcept { socket_.close(); }
	bool is_open() const noexcept { return socket_.is_open(); }
	net::impl::socket_t native_handle() const noexcept { return socket_.native_handle(); }

	/**
	 * @brief Get a reference to the underlying Socket
	 */
	socket_type& socket() noexcept { return socket_; }
	const socket_type& socket() const noexcept { return socket_; }

private:
	socket_type socket_;
};

} // namespace async
} // namespace etherz

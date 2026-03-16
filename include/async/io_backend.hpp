/**
 * @file io_backend.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Platform-agnostic async I/O backend abstraction
 * @version 1.4.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <concepts>

#include "poll.hpp"
#include "../net/socket.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  IoBackend Concept
// ═══════════════════════════════════════════════

/**
 * @brief Completion callback for async I/O operations
 * @param fd Socket file descriptor
 * @param events Events that triggered
 * @param bytes_transferred Number of bytes transferred (0 for non-data ops)
 * @param error Error code if operation failed
 */
using IoCallback = std::function<void(net::impl::socket_t fd, PollEvent events,
	size_t bytes_transferred, core::Error error)>;

/**
 * @brief Concept for I/O backend implementations
 * 
 * All backends must provide these operations:
 * - add/remove socket registration
 * - poll_once for single dispatch cycle
 * - start/stop for continuous operation
 */
template <typename B>
concept IoBackendType = requires(B backend, net::impl::socket_t fd,
	PollEvent interest, EventCallback cb, int timeout_ms)
{
	{ backend.add(fd, interest, cb) } -> std::same_as<void>;
	{ backend.remove(fd) } -> std::same_as<void>;
	{ backend.poll_once(timeout_ms) } -> std::same_as<int>;
	{ backend.stop() } -> std::same_as<void>;
	{ backend.size() } -> std::convertible_to<size_t>;
};

// ═══════════════════════════════════════════════
//  Backend Type Enum
// ═══════════════════════════════════════════════

/**
 * @brief Available I/O backend types
 */
enum class IoBackendKind : uint8_t {
	Poll,       // Default — cross-platform poll()
	Iocp,       // Windows IOCP
	IoUring,    // Linux io_uring
	Kqueue      // macOS/BSD kqueue
};

/**
 * @brief Get the best available backend for the current platform
 */
inline consteval IoBackendKind default_backend() noexcept {
#if defined(_WIN32)
	return IoBackendKind::Iocp;
#elif defined(__linux__)
	return IoBackendKind::IoUring;
#elif defined(__APPLE__) || defined(__FreeBSD__)
	return IoBackendKind::Kqueue;
#else
	return IoBackendKind::Poll;
#endif
}

/**
 * @brief Get the backend name as a string
 */
inline constexpr std::string_view backend_name(IoBackendKind kind) noexcept {
	switch (kind) {
		case IoBackendKind::Poll:    return "poll";
		case IoBackendKind::Iocp:    return "iocp";
		case IoBackendKind::IoUring: return "io_uring";
		case IoBackendKind::Kqueue:  return "kqueue";
	}
	return "unknown";
}

// ═══════════════════════════════════════════════
//  Poll Backend (Default — works everywhere)
// ═══════════════════════════════════════════════

/**
 * @brief Poll-based I/O backend (default, cross-platform)
 * 
 * Uses the existing poll() wrapper. This serves as the reference
 * implementation and fallback for all platforms.
 */
class PollBackend {
public:
	static constexpr IoBackendKind kind = IoBackendKind::Poll;

	PollBackend() noexcept = default;

	void add(net::impl::socket_t fd, PollEvent interest, EventCallback callback) {
		for (auto& reg : registrations_) {
			if (reg.fd == fd) {
				reg.interest = interest;
				reg.callback = std::move(callback);
				return;
			}
		}
		registrations_.push_back({fd, interest, std::move(callback)});
	}

	void remove(net::impl::socket_t fd) noexcept {
		std::erase_if(registrations_, [fd](const auto& r) { return r.fd == fd; });
	}

	int poll_once(int timeout_ms = -1) {
		if (registrations_.empty()) return 0;

		poll_entries_.resize(registrations_.size());
		for (size_t i = 0; i < registrations_.size(); ++i) {
			poll_entries_[i].fd = registrations_[i].fd;
			poll_entries_[i].requested = registrations_[i].interest;
			poll_entries_[i].returned = PollEvent::None;
		}

		int ready = async::poll(poll_entries_, timeout_ms);
		if (ready <= 0) return 0;

		auto snapshot = registrations_;
		int dispatched = 0;
		for (size_t i = 0; i < poll_entries_.size() && i < snapshot.size(); ++i) {
			if (poll_entries_[i].returned != PollEvent::None) {
				if (snapshot[i].callback) {
					snapshot[i].callback(poll_entries_[i].fd, poll_entries_[i].returned);
				}
				++dispatched;
			}
		}
		return dispatched;
	}

	void stop() noexcept { running_ = false; }

	void run(int timeout_ms = 100) {
		running_ = true;
		while (running_ && !registrations_.empty()) {
			poll_once(timeout_ms);
		}
	}

	size_t size() const noexcept { return registrations_.size(); }
	bool empty() const noexcept { return registrations_.empty(); }

private:
	struct Registration {
		net::impl::socket_t fd;
		PollEvent interest;
		EventCallback callback;
	};

	std::vector<Registration> registrations_;
	std::vector<PollEntry> poll_entries_;
	bool running_ = false;
};

} // namespace async
} // namespace etherz

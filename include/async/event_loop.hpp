/**
 * @file event_loop.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Single-threaded event loop for I/O multiplexing
 * @version 1.0.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <print>

#include "poll.hpp"
#include "../net/socket.hpp"

namespace etherz {
namespace async {

/**
 * @brief Callback type for event notifications
 * @param fd The socket file descriptor that triggered
 * @param events The events that occurred
 */
using EventCallback = std::function<void(net::impl::socket_t fd, PollEvent events)>;

/**
 * @brief Single-threaded event loop using poll-based I/O multiplexing
 * 
 * Register sockets with interest events and callbacks. The loop polls
 * all registered sockets and dispatches callbacks when events occur.
 */
class EventLoop {
public:
	EventLoop() noexcept = default;

	/**
	 * @brief Register a socket with interest events and callback
	 * @param fd Socket file descriptor
	 * @param interest Events to monitor (ReadReady, WriteReady, etc.)
	 * @param callback Function to call when events occur
	 */
	void add(net::impl::socket_t fd, PollEvent interest, EventCallback callback) {
		// Update existing entry if fd already registered
		for (auto& reg : registrations_) {
			if (reg.fd == fd) {
				reg.interest = interest;
				reg.callback = std::move(callback);
				return;
			}
		}
		registrations_.push_back({fd, interest, std::move(callback)});
	}

	/**
	 * @brief Unregister a socket from the event loop
	 */
	void remove(net::impl::socket_t fd) noexcept {
		std::erase_if(registrations_, [fd](const auto& r) { return r.fd == fd; });
	}

	/**
	 * @brief Run a single poll + dispatch cycle
	 * @param timeout_ms Timeout in milliseconds (-1 = block, 0 = non-blocking)
	 * @return Number of events dispatched
	 */
	int run_once(int timeout_ms = -1) {
		if (registrations_.empty()) return 0;

		// Build poll entries
		poll_entries_.resize(registrations_.size());
		for (size_t i = 0; i < registrations_.size(); ++i) {
			poll_entries_[i].fd = registrations_[i].fd;
			poll_entries_[i].requested = registrations_[i].interest;
			poll_entries_[i].returned = PollEvent::None;
		}

		int ready = async::poll(poll_entries_, timeout_ms);
		if (ready <= 0) return 0;

		// Snapshot registrations to avoid iterator invalidation
		// when callbacks call add()/remove()
		auto snapshot = registrations_;

		int dispatched = 0;
		for (size_t i = 0; i < poll_entries_.size() && i < snapshot.size(); ++i) {
			if (poll_entries_[i].returned != PollEvent::None) {
				if (snapshot[i].callback) {
					snapshot[i].callback(
						poll_entries_[i].fd,
						poll_entries_[i].returned
					);
				}
				++dispatched;
			}
		}

		return dispatched;
	}

	/**
	 * @brief Run the event loop continuously until stop() is called
	 * @param timeout_ms Timeout per poll cycle
	 */
	void run(int timeout_ms = 100) {
		running_ = true;
		while (running_ && !registrations_.empty()) {
			run_once(timeout_ms);
		}
	}

	/**
	 * @brief Stop the event loop after current cycle completes
	 */
	void stop() noexcept {
		running_ = false;
	}

	/**
	 * @brief Check if the event loop is currently running
	 */
	bool is_running() const noexcept { return running_; }

	/**
	 * @brief Get number of registered sockets
	 */
	size_t size() const noexcept { return registrations_.size(); }

	/**
	 * @brief Check if no sockets are registered
	 */
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

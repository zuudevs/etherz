/**
 * @file io_uring_backend.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Linux io_uring async I/O backend
 * @version 1.4.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "io_backend.hpp"

#ifdef __linux__

#include <liburing.h>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>

namespace etherz {
namespace async {

/**
 * @brief io_uring-based async I/O backend for Linux
 * 
 * Uses the io_uring kernel interface for high-performance,
 * zero-copy async I/O on modern Linux kernels (5.1+).
 */
class IoUringBackend {
public:
	static constexpr IoBackendKind kind = IoBackendKind::IoUring;
	static constexpr unsigned QUEUE_DEPTH = 256;

	IoUringBackend() noexcept {
		initialized_ = (io_uring_queue_init(QUEUE_DEPTH, &ring_, 0) == 0);
	}

	~IoUringBackend() noexcept {
		if (initialized_) {
			io_uring_queue_exit(&ring_);
		}
	}

	// Non-copyable
	IoUringBackend(const IoUringBackend&) = delete;
	IoUringBackend& operator=(const IoUringBackend&) = delete;

	/**
	 * @brief Register a socket for poll monitoring via io_uring
	 */
	void add(net::impl::socket_t fd, PollEvent interest, EventCallback callback) {
		for (auto& reg : registrations_) {
			if (reg.fd == fd) {
				reg.interest = interest;
				reg.callback = std::move(callback);
				submit_poll(fd, interest);
				return;
			}
		}
		registrations_.push_back({fd, interest, std::move(callback)});
		submit_poll(fd, interest);
	}

	/**
	 * @brief Remove socket registration
	 */
	void remove(net::impl::socket_t fd) noexcept {
		std::erase_if(registrations_, [fd](const auto& r) { return r.fd == fd; });
	}

	/**
	 * @brief Wait for and dispatch one batch of completions
	 */
	int poll_once(int timeout_ms = -1) {
		if (!initialized_ || registrations_.empty()) return 0;

		struct io_uring_cqe* cqe = nullptr;
		int ret;

		if (timeout_ms < 0) {
			ret = io_uring_wait_cqe(&ring_, &cqe);
		} else if (timeout_ms == 0) {
			ret = io_uring_peek_cqe(&ring_, &cqe);
		} else {
			struct __kernel_timespec ts;
			ts.tv_sec = timeout_ms / 1000;
			ts.tv_nsec = (timeout_ms % 1000) * 1000000LL;
			ret = io_uring_wait_cqe_timeout(&ring_, &cqe, &ts);
		}

		if (ret < 0 || cqe == nullptr) return 0;

		auto fd = static_cast<net::impl::socket_t>(cqe->user_data);
		PollEvent events = PollEvent::None;

		if (cqe->res >= 0) {
			// Convert poll mask to PollEvent
			if (cqe->res & POLLIN) events |= PollEvent::ReadReady;
			if (cqe->res & POLLOUT) events |= PollEvent::WriteReady;
			if (cqe->res & POLLERR) events |= PollEvent::Error;
			if (cqe->res & POLLHUP) events |= PollEvent::HangUp;
		} else {
			events = PollEvent::Error;
		}

		io_uring_cqe_seen(&ring_, cqe);

		// Dispatch callback
		int dispatched = 0;
		for (const auto& reg : registrations_) {
			if (reg.fd == fd && reg.callback) {
				reg.callback(fd, events);
				++dispatched;
				// Resubmit poll for persistent monitoring
				submit_poll(fd, reg.interest);
				break;
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
	bool is_initialized() const noexcept { return initialized_; }

private:
	struct Registration {
		net::impl::socket_t fd;
		PollEvent interest;
		EventCallback callback;
	};

	struct io_uring ring_{};
	std::vector<Registration> registrations_;
	bool initialized_ = false;
	bool running_ = false;

	/**
	 * @brief Submit a poll SQE for the given socket
	 */
	void submit_poll(net::impl::socket_t fd, PollEvent interest) {
		if (!initialized_) return;

		struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
		if (!sqe) return;

		short poll_mask = 0;
		if (has_event(interest, PollEvent::ReadReady)) poll_mask |= POLLIN;
		if (has_event(interest, PollEvent::WriteReady)) poll_mask |= POLLOUT;

		io_uring_prep_poll_add(sqe, fd, poll_mask);
		io_uring_sqe_set_data64(sqe, static_cast<uint64_t>(fd));
		io_uring_submit(&ring_);
	}
};

} // namespace async
} // namespace etherz

#endif // __linux__

/**
 * @file kqueue_backend.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief macOS/BSD kqueue async I/O backend
 * @version 1.4.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "io_backend.hpp"

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <functional>
#include <algorithm>

namespace etherz {
namespace async {

/**
 * @brief kqueue-based async I/O backend for macOS/BSD
 * 
 * Uses kqueue() and kevent() for high-performance event notification.
 */
class KqueueBackend {
public:
	static constexpr IoBackendKind kind = IoBackendKind::Kqueue;

	KqueueBackend() noexcept {
		kq_ = ::kqueue();
	}

	~KqueueBackend() noexcept {
		if (kq_ >= 0) {
			::close(kq_);
		}
	}

	// Non-copyable
	KqueueBackend(const KqueueBackend&) = delete;
	KqueueBackend& operator=(const KqueueBackend&) = delete;

	/**
	 * @brief Register a socket with kqueue
	 */
	void add(net::impl::socket_t fd, PollEvent interest, EventCallback callback) {
		// Remove existing registration first
		remove(fd);

		registrations_.push_back({fd, interest, std::move(callback)});

		// Register kqueue events
		struct kevent changes[2];
		int nchanges = 0;

		if (has_event(interest, PollEvent::ReadReady)) {
			EV_SET(&changes[nchanges++], static_cast<uintptr_t>(fd),
				EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
		}
		if (has_event(interest, PollEvent::WriteReady)) {
			EV_SET(&changes[nchanges++], static_cast<uintptr_t>(fd),
				EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
		}

		if (nchanges > 0 && kq_ >= 0) {
			::kevent(kq_, changes, nchanges, nullptr, 0, nullptr);
		}
	}

	/**
	 * @brief Remove socket from kqueue
	 */
	void remove(net::impl::socket_t fd) noexcept {
		auto it = std::find_if(registrations_.begin(), registrations_.end(),
			[fd](const auto& r) { return r.fd == fd; });

		if (it != registrations_.end() && kq_ >= 0) {
			struct kevent changes[2];
			EV_SET(&changes[0], static_cast<uintptr_t>(fd),
				EVFILT_READ, EV_DELETE, 0, 0, nullptr);
			EV_SET(&changes[1], static_cast<uintptr_t>(fd),
				EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
			::kevent(kq_, changes, 2, nullptr, 0, nullptr);
		}

		std::erase_if(registrations_, [fd](const auto& r) { return r.fd == fd; });
	}

	/**
	 * @brief Wait for and dispatch one batch of events
	 */
	int poll_once(int timeout_ms = -1) {
		if (kq_ < 0 || registrations_.empty()) return 0;

		struct kevent events[64];
		struct timespec ts;
		struct timespec* ts_ptr = nullptr;

		if (timeout_ms >= 0) {
			ts.tv_sec = timeout_ms / 1000;
			ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
			ts_ptr = &ts;
		}

		int nev = ::kevent(kq_, nullptr, 0, events, 64, ts_ptr);
		if (nev <= 0) return 0;

		int dispatched = 0;
		for (int i = 0; i < nev; ++i) {
			auto fd = static_cast<net::impl::socket_t>(events[i].ident);
			PollEvent poll_events = PollEvent::None;

			if (events[i].filter == EVFILT_READ)  poll_events |= PollEvent::ReadReady;
			if (events[i].filter == EVFILT_WRITE) poll_events |= PollEvent::WriteReady;
			if (events[i].flags & EV_EOF)         poll_events |= PollEvent::HangUp;
			if (events[i].flags & EV_ERROR)       poll_events |= PollEvent::Error;

			for (const auto& reg : registrations_) {
				if (reg.fd == fd && reg.callback) {
					reg.callback(fd, poll_events);
					++dispatched;
					break;
				}
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

	int kq_ = -1;
	std::vector<Registration> registrations_;
	bool running_ = false;
};

} // namespace async
} // namespace etherz

#endif // __APPLE__ || __FreeBSD__

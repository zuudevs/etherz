/**
 * @file iocp_backend.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Windows I/O Completion Port (IOCP) backend
 * @version 1.4.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "io_backend.hpp"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32.lib")

#include <vector>
#include <functional>
#include <algorithm>

namespace etherz {
namespace async {

/**
 * @brief IOCP-based async I/O backend for Windows
 * 
 * Uses CreateIoCompletionPort and GetQueuedCompletionStatus
 * for high-performance, scalable I/O multiplexing on Windows.
 */
class IocpBackend {
public:
	static constexpr IoBackendKind kind = IoBackendKind::Iocp;

	IocpBackend() noexcept {
		iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	}

	~IocpBackend() noexcept {
		if (iocp_ != nullptr && iocp_ != INVALID_HANDLE_VALUE) {
			CloseHandle(iocp_);
		}
	}

	// Non-copyable
	IocpBackend(const IocpBackend&) = delete;
	IocpBackend& operator=(const IocpBackend&) = delete;

	/**
	 * @brief Associate a socket with the IOCP and register events
	 */
	void add(net::impl::socket_t fd, PollEvent interest, EventCallback callback) {
		// Associate socket with IOCP
		CreateIoCompletionPort(
			reinterpret_cast<HANDLE>(fd),
			iocp_,
			static_cast<ULONG_PTR>(fd),
			0
		);

		// Store registration for callback dispatch
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
	 * @brief Remove socket registration
	 */
	void remove(net::impl::socket_t fd) noexcept {
		std::erase_if(registrations_, [fd](const auto& r) { return r.fd == fd; });
	}

	/**
	 * @brief Poll for one batch of completions
	 * @param timeout_ms Timeout (-1 = infinite, 0 = non-blocking)
	 * @return Number of events dispatched
	 */
	int poll_once(int timeout_ms = -1) {
		if (registrations_.empty()) return 0;

		DWORD bytes_transferred = 0;
		ULONG_PTR completion_key = 0;
		LPOVERLAPPED overlapped = nullptr;

		DWORD timeout = (timeout_ms < 0) ? INFINITE : static_cast<DWORD>(timeout_ms);

		BOOL success = GetQueuedCompletionStatus(
			iocp_,
			&bytes_transferred,
			&completion_key,
			&overlapped,
			timeout
		);

		if (!success && overlapped == nullptr) {
			// Timeout or error
			return 0;
		}

		auto fd = static_cast<net::impl::socket_t>(completion_key);
		PollEvent events = success
			? (PollEvent::ReadReady | PollEvent::WriteReady)
			: PollEvent::Error;

		// Dispatch to registered callback
		for (const auto& reg : registrations_) {
			if (reg.fd == fd && reg.callback) {
				reg.callback(fd, events);
				return 1;
			}
		}

		return 0;
	}

	void stop() noexcept {
		running_ = false;
		// Post a dummy completion to wake up GetQueuedCompletionStatus
		if (iocp_ != nullptr && iocp_ != INVALID_HANDLE_VALUE) {
			PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
		}
	}

	void run(int timeout_ms = 100) {
		running_ = true;
		while (running_ && !registrations_.empty()) {
			poll_once(timeout_ms);
		}
	}

	size_t size() const noexcept { return registrations_.size(); }
	bool empty() const noexcept { return registrations_.empty(); }

	/**
	 * @brief Get the native IOCP handle
	 */
	HANDLE native_handle() const noexcept { return iocp_; }

private:
	struct Registration {
		net::impl::socket_t fd;
		PollEvent interest;
		EventCallback callback;
	};

	HANDLE iocp_ = INVALID_HANDLE_VALUE;
	std::vector<Registration> registrations_;
	bool running_ = false;
};

} // namespace async
} // namespace etherz

#endif // _WIN32

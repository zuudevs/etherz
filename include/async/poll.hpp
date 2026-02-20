/**
 * @file poll.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief I/O multiplexing poll wrapper
 * @version 1.0.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include "../core/error.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include <poll.h>
#endif

namespace etherz {
namespace async {

/**
 * @brief Bitmask flags for poll events
 */
enum class PollEvent : uint8_t {
	None       = 0,
	ReadReady  = 1 << 0,   // Data available to read
	WriteReady = 1 << 1,   // Socket ready for writing
	Error      = 1 << 2,   // Error condition
	HangUp     = 1 << 3    // Peer closed connection
};

inline constexpr PollEvent operator|(PollEvent a, PollEvent b) noexcept {
	return static_cast<PollEvent>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr PollEvent operator&(PollEvent a, PollEvent b) noexcept {
	return static_cast<PollEvent>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

inline constexpr PollEvent& operator|=(PollEvent& a, PollEvent b) noexcept {
	a = a | b;
	return a;
}

inline constexpr bool has_event(PollEvent flags, PollEvent test) noexcept {
	return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

/**
 * @brief Entry for a poll operation
 */
struct PollEntry {
	net::impl::socket_t fd = net::impl::invalid_socket;
	PollEvent requested = PollEvent::None;   // Events to watch for
	PollEvent returned  = PollEvent::None;   // Events that occurred
};

namespace impl {

// Platform-independent pollfd type alias
#ifdef _WIN32
using native_pollfd = WSAPOLLFD;
#else
using native_pollfd = struct pollfd;
#endif

/**
 * @brief Convert PollEvent to platform poll flags
 */
inline short to_native_events(PollEvent ev) noexcept {
	short flags = 0;
	if (has_event(ev, PollEvent::ReadReady))  flags |= POLLIN;
	if (has_event(ev, PollEvent::WriteReady)) flags |= POLLOUT;
	return flags;
}

/**
 * @brief Convert platform poll flags to PollEvent
 */
inline PollEvent from_native_events(short revents) noexcept {
	PollEvent ev = PollEvent::None;
	if (revents & POLLIN)  ev |= PollEvent::ReadReady;
	if (revents & POLLOUT) ev |= PollEvent::WriteReady;
	if (revents & POLLERR) ev |= PollEvent::Error;
	if (revents & POLLHUP) ev |= PollEvent::HangUp;
	return ev;
}

} // namespace impl

/**
 * @brief Poll a set of sockets for I/O readiness
 * 
 * @param entries Span of PollEntry to watch
 * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 = non-blocking)
 * @return Number of ready entries, or -1 on error
 */
inline int poll(std::span<PollEntry> entries, int timeout_ms) noexcept {
	if (entries.empty()) return 0;

	// Build native pollfd array (stack-allocate for small counts)
	constexpr size_t STACK_MAX = 64;
	impl::native_pollfd stack_fds[STACK_MAX];
	
	auto* fds = stack_fds;
	std::unique_ptr<impl::native_pollfd[]> heap_fds;

	if (entries.size() > STACK_MAX) {
		heap_fds = std::make_unique<impl::native_pollfd[]>(entries.size());
		fds = heap_fds.get();
	}

	for (size_t i = 0; i < entries.size(); ++i) {
		fds[i].fd      = entries[i].fd;
		fds[i].events  = impl::to_native_events(entries[i].requested);
		fds[i].revents = 0;
	}

#ifdef _WIN32
	int result = WSAPoll(fds, static_cast<ULONG>(entries.size()), timeout_ms);
#else
	int result = ::poll(fds, static_cast<nfds_t>(entries.size()), timeout_ms);
#endif

	if (result > 0) {
		for (size_t i = 0; i < entries.size(); ++i) {
			entries[i].returned = impl::from_native_events(fds[i].revents);
		}
	}

	return result;
}

/**
 * @brief Get a human-readable name for a PollEvent
 */
inline constexpr std::string_view poll_event_name(PollEvent ev) noexcept {
	switch (ev) {
		case PollEvent::None:       return "None";
		case PollEvent::ReadReady:  return "ReadReady";
		case PollEvent::WriteReady: return "WriteReady";
		case PollEvent::Error:      return "Error";
		case PollEvent::HangUp:     return "HangUp";
		default:                    return "Mixed";
	}
}

} // namespace async
} // namespace etherz

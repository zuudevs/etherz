/**
 * @file rate_limiter.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Token-bucket rate limiter and throttled socket wrapper
 * @version 1.3.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <algorithm>
#include <span>
#include <expected>

#include "socket.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Token-Bucket Rate Limiter
// ═══════════════════════════════════════════════

/**
 * @brief Token-bucket rate limiter
 * 
 * Allows a configurable rate of operations with burst capacity.
 * Tokens are replenished continuously based on elapsed time.
 * 
 * Can be used for:
 * - Bytes per second (bandwidth limiting)
 * - Requests per second (API rate limiting)
 */
class RateLimiter {
public:
	using clock_type = std::chrono::steady_clock;
	using time_point = clock_type::time_point;

	/**
	 * @brief Construct a rate limiter
	 * @param rate Tokens per second (e.g., bytes/sec or requests/sec)
	 * @param burst Maximum burst size (token bucket capacity)
	 */
	RateLimiter(double rate, double burst) noexcept
		: rate_(rate)
		, burst_(burst)
		, tokens_(burst)
		, last_refill_(clock_type::now()) {}

	/**
	 * @brief Try to consume tokens
	 * @param count Number of tokens to consume
	 * @return true if tokens were consumed, false if insufficient
	 */
	bool try_consume(double count = 1.0) noexcept {
		refill();
		if (tokens_ >= count) {
			tokens_ -= count;
			return true;
		}
		return false;
	}

	/**
	 * @brief Consume tokens, blocking until available
	 * 
	 * Note: Uses busy-wait — suitable for short waits only.
	 * For long waits, prefer try_consume() with your own timing.
	 */
	void consume(double count = 1.0) noexcept {
		while (!try_consume(count)) {
			// Yield a small amount of time
			refill();
		}
	}

	/**
	 * @brief Get the estimated wait time for the given tokens
	 * @return Milliseconds to wait, 0 if tokens available now
	 */
	uint32_t wait_time_ms(double count = 1.0) noexcept {
		refill();
		if (tokens_ >= count) return 0;
		double deficit = count - tokens_;
		double seconds = deficit / rate_;
		return static_cast<uint32_t>(seconds * 1000.0 + 0.5);
	}

	/**
	 * @brief Get current available tokens
	 */
	double available() noexcept {
		refill();
		return tokens_;
	}

	/**
	 * @brief Reset the limiter to full burst capacity
	 */
	void reset() noexcept {
		tokens_ = burst_;
		last_refill_ = clock_type::now();
	}

	// ─── Configuration ──────────────────

	double rate() const noexcept { return rate_; }
	double burst() const noexcept { return burst_; }

	void set_rate(double rate) noexcept { rate_ = rate; }
	void set_burst(double burst) noexcept {
		burst_ = burst;
		if (tokens_ > burst_) tokens_ = burst_;
	}

private:
	double     rate_;          // Tokens per second
	double     burst_;         // Maximum capacity
	double     tokens_;        // Current available tokens
	time_point last_refill_;   // Last refill timestamp

	/**
	 * @brief Refill tokens based on elapsed time
	 */
	void refill() noexcept {
		auto now = clock_type::now();
		auto elapsed = std::chrono::duration<double>(now - last_refill_).count();
		tokens_ = std::min(burst_, tokens_ + elapsed * rate_);
		last_refill_ = now;
	}
};

// ═══════════════════════════════════════════════
//  Throttled Socket Wrapper
// ═══════════════════════════════════════════════

/**
 * @brief Socket wrapper that enforces send/recv rate limits
 * 
 * Wraps a Socket<T> and uses RateLimiter to throttle data transfer.
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class ThrottledSocket {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>,
		"Invalid IP version.");

public:
	using address_type = SocketAddress<T>;

	/**
	 * @brief Construct with rate limits
	 * @param send_bps Send rate limit in bytes per second (0 = unlimited)
	 * @param recv_bps Receive rate limit in bytes per second (0 = unlimited)
	 */
	ThrottledSocket(double send_bps = 0, double recv_bps = 0) noexcept
		: send_limiter_(send_bps > 0 ? send_bps : 1e18, send_bps > 0 ? send_bps : 1e18)
		, recv_limiter_(recv_bps > 0 ? recv_bps : 1e18, recv_bps > 0 ? recv_bps : 1e18)
		, send_limited_(send_bps > 0)
		, recv_limited_(recv_bps > 0) {}

	~ThrottledSocket() noexcept = default;

	// Non-copyable, movable
	ThrottledSocket(const ThrottledSocket&) = delete;
	ThrottledSocket& operator=(const ThrottledSocket&) = delete;
	ThrottledSocket(ThrottledSocket&&) noexcept = default;
	ThrottledSocket& operator=(ThrottledSocket&&) noexcept = default;

	// ─── Lifecycle ──────────────────────

	core::Error create() noexcept { return socket_.create(); }

	core::Error bind(const address_type& addr) noexcept {
		return socket_.bind(addr);
	}

	core::Error connect(const address_type& addr) noexcept {
		return socket_.connect(addr);
	}

	/**
	 * @brief Send data with rate limiting
	 * 
	 * Blocks until sufficient bandwidth tokens are available,
	 * then sends the data.
	 */
	int send(std::span<const uint8_t> data) noexcept {
		if (send_limited_) {
			if (!send_limiter_.try_consume(static_cast<double>(data.size()))) {
				return -1;  // Rate limited — caller should retry
			}
		}
		int sent = socket_.send(data);
		if (sent > 0) {
			bytes_sent_ += static_cast<size_t>(sent);
		}
		return sent;
	}

	/**
	 * @brief Receive data with rate limiting
	 */
	int recv(std::span<uint8_t> buffer) noexcept {
		if (recv_limited_) {
			if (!recv_limiter_.try_consume(static_cast<double>(buffer.size()))) {
				return -1;  // Rate limited
			}
		}
		int received = socket_.recv(buffer);
		if (received > 0) {
			bytes_received_ += static_cast<size_t>(received);
		}
		return received;
	}

	void close() noexcept { socket_.close(); }

	// ─── Rate Control ───────────────────

	/**
	 * @brief Set send rate limit in bytes per second
	 */
	void set_send_rate(double bps) noexcept {
		send_limited_ = (bps > 0);
		if (send_limited_) {
			send_limiter_.set_rate(bps);
			send_limiter_.set_burst(bps);
		}
	}

	/**
	 * @brief Set receive rate limit in bytes per second
	 */
	void set_recv_rate(double bps) noexcept {
		recv_limited_ = (bps > 0);
		if (recv_limited_) {
			recv_limiter_.set_rate(bps);
			recv_limiter_.set_burst(bps);
		}
	}

	// ─── Statistics ─────────────────────

	size_t bytes_sent() const noexcept { return bytes_sent_; }
	size_t bytes_received() const noexcept { return bytes_received_; }

	void reset_stats() noexcept {
		bytes_sent_ = 0;
		bytes_received_ = 0;
	}

	// ─── Queries ────────────────────────

	bool is_open() const noexcept { return socket_.is_open(); }
	Socket<T>& socket() noexcept { return socket_; }
	const Socket<T>& socket() const noexcept { return socket_; }

private:
	Socket<T>   socket_;
	RateLimiter send_limiter_;
	RateLimiter recv_limiter_;
	bool        send_limited_ = false;
	bool        recv_limited_ = false;
	size_t      bytes_sent_     = 0;
	size_t      bytes_received_ = 0;
};

} // namespace net
} // namespace etherz

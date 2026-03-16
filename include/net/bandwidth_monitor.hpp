/**
 * @file bandwidth_monitor.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Bandwidth monitoring and statistics tracking
 * @version 1.3.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <print>

namespace etherz {
namespace net {

/**
 * @brief Tracks bandwidth usage over time
 * 
 * Records bytes sent/received and computes throughput rates
 * over configurable time windows.
 */
class BandwidthMonitor {
public:
	using clock_type = std::chrono::steady_clock;
	using time_point = clock_type::time_point;

	BandwidthMonitor() noexcept
		: start_time_(clock_type::now())
		, window_start_(clock_type::now()) {}

	// ─── Recording ──────────────────────

	/**
	 * @brief Record bytes sent
	 */
	void record_sent(size_t bytes) noexcept {
		total_sent_ += bytes;
		window_sent_ += bytes;
	}

	/**
	 * @brief Record bytes received
	 */
	void record_received(size_t bytes) noexcept {
		total_received_ += bytes;
		window_received_ += bytes;
	}

	// ─── Statistics ─────────────────────

	/**
	 * @brief Get total bytes sent since construction
	 */
	size_t total_sent() const noexcept { return total_sent_; }

	/**
	 * @brief Get total bytes received since construction
	 */
	size_t total_received() const noexcept { return total_received_; }

	/**
	 * @brief Get total bytes transferred (sent + received)
	 */
	size_t total_bytes() const noexcept { return total_sent_ + total_received_; }

	/**
	 * @brief Get elapsed time since construction in seconds
	 */
	double elapsed_seconds() const noexcept {
		auto now = clock_type::now();
		return std::chrono::duration<double>(now - start_time_).count();
	}

	/**
	 * @brief Get average send rate in bytes per second (lifetime)
	 */
	double avg_send_rate() const noexcept {
		double elapsed = elapsed_seconds();
		return (elapsed > 0) ? static_cast<double>(total_sent_) / elapsed : 0.0;
	}

	/**
	 * @brief Get average receive rate in bytes per second (lifetime)
	 */
	double avg_recv_rate() const noexcept {
		double elapsed = elapsed_seconds();
		return (elapsed > 0) ? static_cast<double>(total_received_) / elapsed : 0.0;
	}

	// ─── Windowed Rates ─────────────────

	/**
	 * @brief Get send rate over the current window and reset window
	 * @return Bytes per second over the last window
	 */
	double window_send_rate() noexcept {
		auto now = clock_type::now();
		double elapsed = std::chrono::duration<double>(now - window_start_).count();
		double rate = (elapsed > 0) ? static_cast<double>(window_sent_) / elapsed : 0.0;
		window_sent_ = 0;
		window_start_ = now;
		return rate;
	}

	/**
	 * @brief Get receive rate over the current window and reset window
	 * @return Bytes per second over the last window
	 */
	double window_recv_rate() noexcept {
		auto now = clock_type::now();
		double elapsed = std::chrono::duration<double>(now - window_start_).count();
		double rate = (elapsed > 0) ? static_cast<double>(window_received_) / elapsed : 0.0;
		window_received_ = 0;
		window_start_ = now;
		return rate;
	}

	// ─── Formatting ─────────────────────

	/**
	 * @brief Format bytes into human-readable string
	 */
	static std::string format_bytes(size_t bytes) {
		if (bytes >= 1073741824) {
			return std::to_string(bytes / 1073741824) + "."
				+ std::to_string((bytes % 1073741824) * 10 / 1073741824) + " GB";
		}
		if (bytes >= 1048576) {
			return std::to_string(bytes / 1048576) + "."
				+ std::to_string((bytes % 1048576) * 10 / 1048576) + " MB";
		}
		if (bytes >= 1024) {
			return std::to_string(bytes / 1024) + "."
				+ std::to_string((bytes % 1024) * 10 / 1024) + " KB";
		}
		return std::to_string(bytes) + " B";
	}

	/**
	 * @brief Format rate into human-readable string
	 */
	static std::string format_rate(double bps) {
		return format_bytes(static_cast<size_t>(bps)) + "/s";
	}

	/**
	 * @brief Print a summary of bandwidth statistics
	 */
	void display() const noexcept {
		std::print("Bandwidth: tx={} rx={} elapsed={:.1f}s avg_tx={}/s avg_rx={}/s\n",
			format_bytes(total_sent_), format_bytes(total_received_),
			elapsed_seconds(),
			format_bytes(static_cast<size_t>(avg_send_rate())),
			format_bytes(static_cast<size_t>(avg_recv_rate())));
	}

	// ─── Reset ──────────────────────────

	void reset() noexcept {
		total_sent_ = 0;
		total_received_ = 0;
		window_sent_ = 0;
		window_received_ = 0;
		start_time_ = clock_type::now();
		window_start_ = clock_type::now();
	}

private:
	size_t     total_sent_       = 0;
	size_t     total_received_   = 0;
	size_t     window_sent_      = 0;
	size_t     window_received_  = 0;
	time_point start_time_;
	time_point window_start_;
};

} // namespace net
} // namespace etherz

/**
 * @file timer.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Async timer with one-shot, repeating, and co_await support
 * @version 2.1.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <chrono>
#include <functional>
#include <coroutine>
#include <thread>
#include <atomic>

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  Timer
// ═══════════════════════════════════════════════

/**
 * @brief Async timer with one-shot and repeating modes
 * 
 * Usage (callback):
 *   Timer timer;
 *   timer.set_timeout(1000, [] { std::print("Fired!\n"); });
 *   timer.set_interval(500, [] { std::print("Tick\n"); });
 * 
 * Usage (coroutine):
 *   co_await Timer::delay(std::chrono::milliseconds(100));
 */
class Timer {
public:
	using clock_type = std::chrono::steady_clock;
	using duration   = std::chrono::milliseconds;
	using callback_t = std::function<void()>;

	Timer() noexcept = default;

	~Timer() noexcept {
		cancel();
	}

	// Non-copyable
	Timer(const Timer&) = delete;
	Timer& operator=(const Timer&) = delete;

	Timer(Timer&& other) noexcept
		: running_(other.running_.load())
		, cancelled_(other.cancelled_.load())
	{
		other.cancelled_ = true;
	}

	// ─── One-Shot Timer ─────────────────

	/**
	 * @brief Execute callback after delay (non-blocking, spawns thread)
	 * @param ms Delay in milliseconds
	 * @param callback Function to call
	 */
	void set_timeout(uint32_t ms, callback_t callback) {
		cancel();
		cancelled_ = false;
		running_ = true;

		std::thread([this, ms, cb = std::move(callback)]() {
			auto target = clock_type::now() + std::chrono::milliseconds(ms);
			while (!cancelled_ && clock_type::now() < target) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
			if (!cancelled_) {
				cb();
			}
			running_ = false;
		}).detach();
	}

	// ─── Repeating Timer ────────────────

	/**
	 * @brief Execute callback at regular intervals (non-blocking)
	 * @param ms Interval in milliseconds
	 * @param callback Function to call each interval
	 */
	void set_interval(uint32_t ms, callback_t callback) {
		cancel();
		cancelled_ = false;
		running_ = true;

		std::thread([this, ms, cb = std::move(callback)]() {
			while (!cancelled_) {
				std::this_thread::sleep_for(std::chrono::milliseconds(ms));
				if (!cancelled_) {
					cb();
				}
			}
			running_ = false;
		}).detach();
	}

	// ─── Control ────────────────────────

	/**
	 * @brief Cancel the timer
	 */
	void cancel() noexcept {
		cancelled_ = true;
		// Wait briefly for thread to finish
		int attempts = 0;
		while (running_ && attempts < 100) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			++attempts;
		}
	}

	bool is_running() const noexcept { return running_; }

	// ─── Coroutine Awaitable ────────────

	/**
	 * @brief Awaitable that delays for a given duration
	 * 
	 * Usage:
	 *   co_await Timer::delay(std::chrono::milliseconds(500));
	 *   co_await Timer::delay(std::chrono::seconds(2));
	 */
	template <typename Duration>
	static auto delay(Duration d) {
		struct DelayAwaitable {
			Duration duration_;

			bool await_ready() const noexcept { return duration_.count() <= 0; }

			void await_suspend(std::coroutine_handle<> h) const {
				std::thread([h, dur = duration_]() {
					std::this_thread::sleep_for(dur);
					h.resume();
				}).detach();
			}

			void await_resume() const noexcept {}
		};

		return DelayAwaitable{d};
	}

	/**
	 * @brief Await a specific time point
	 */
	static auto delay_until(clock_type::time_point tp) {
		auto now = clock_type::now();
		if (tp <= now) {
			return delay(std::chrono::milliseconds(0));
		}
		return delay(std::chrono::duration_cast<std::chrono::milliseconds>(tp - now));
	}

	// ─── Utility ────────────────────────

	/**
	 * @brief Blocking sleep (convenience wrapper)
	 */
	static void sleep(uint32_t ms) {
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	/**
	 * @brief Get monotonic timestamp in milliseconds
	 */
	static uint64_t now_ms() noexcept {
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				clock_type::now().time_since_epoch()).count());
	}

private:
	std::atomic<bool> running_{false};
	std::atomic<bool> cancelled_{false};
};

} // namespace async
} // namespace etherz

/**
 * @file generator.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Coroutine-based lazy Generator<T> for streaming data
 * @version 2.1.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <coroutine>
#include <exception>
#include <utility>
#include <iterator>
#include <cstddef>

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  Generator<T> — Lazy Sequence Generator
// ═══════════════════════════════════════════════

/**
 * @brief Coroutine-based lazy sequence generator
 * 
 * Produces values on demand via `co_yield`. Supports range-based
 * for loops and manual iteration via begin()/end().
 * 
 * Usage:
 *   Generator<int> range(int start, int end) {
 *       for (int i = start; i < end; ++i)
 *           co_yield i;
 *   }
 *   
 *   for (int val : range(0, 10)) {
 *       std::print("{}\n", val);
 *   }
 */
template <typename T>
class Generator {
public:
	struct promise_type {
		T current_value_;
		std::exception_ptr exception_;

		Generator get_return_object() {
			return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
		}

		std::suspend_always initial_suspend() noexcept { return {}; }
		std::suspend_always final_suspend() noexcept { return {}; }

		std::suspend_always yield_value(T value) {
			current_value_ = std::move(value);
			return {};
		}

		void return_void() noexcept {}

		void unhandled_exception() {
			exception_ = std::current_exception();
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	// ─── Iterator ───────────────────────

	class iterator {
	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = const T*;
		using reference = const T&;

		iterator() noexcept = default;
		explicit iterator(handle_type h) noexcept : handle_(h) {}

		reference operator*() const noexcept {
			return handle_.promise().current_value_;
		}

		pointer operator->() const noexcept {
			return &handle_.promise().current_value_;
		}

		iterator& operator++() {
			handle_.resume();
			if (handle_.done()) {
				if (handle_.promise().exception_) {
					std::rethrow_exception(handle_.promise().exception_);
				}
				handle_ = nullptr;
			}
			return *this;
		}

		iterator operator++(int) {
			auto tmp = *this;
			++*this;
			return tmp;
		}

		bool operator==(const iterator& other) const noexcept {
			return handle_ == other.handle_;
		}

		bool operator!=(const iterator& other) const noexcept {
			return !(*this == other);
		}

	private:
		handle_type handle_ = nullptr;
	};

	// ─── Construction ───────────────────

	Generator() noexcept = default;
	explicit Generator(handle_type h) noexcept : handle_(h) {}

	~Generator() {
		if (handle_) handle_.destroy();
	}

	// Non-copyable, movable
	Generator(const Generator&) = delete;
	Generator& operator=(const Generator&) = delete;

	Generator(Generator&& other) noexcept : handle_(other.handle_) {
		other.handle_ = nullptr;
	}

	Generator& operator=(Generator&& other) noexcept {
		if (this != &other) {
			if (handle_) handle_.destroy();
			handle_ = other.handle_;
			other.handle_ = nullptr;
		}
		return *this;
	}

	// ─── Range Interface ────────────────

	iterator begin() {
		if (handle_) {
			handle_.resume();
			if (handle_.done()) {
				return iterator{nullptr};
			}
		}
		return iterator{handle_};
	}

	iterator end() noexcept {
		return iterator{nullptr};
	}

	// ─── Manual Iteration ───────────────

	/**
	 * @brief Advance to the next value
	 * @return true if a value is available, false if exhausted
	 */
	bool next() {
		if (!handle_ || handle_.done()) return false;
		handle_.resume();
		return !handle_.done();
	}

	/**
	 * @brief Get the current value (call after next())
	 */
	const T& value() const noexcept {
		return handle_.promise().current_value_;
	}

	/**
	 * @brief Check if the generator is exhausted
	 */
	bool done() const noexcept {
		return !handle_ || handle_.done();
	}

private:
	handle_type handle_ = nullptr;
};

} // namespace async
} // namespace etherz

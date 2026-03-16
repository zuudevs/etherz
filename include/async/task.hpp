/**
 * @file task.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Lazy coroutine Task<T> type for async I/O operations
 * @version 2.1.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <coroutine>
#include <variant>
#include <exception>
#include <utility>
#include <functional>

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  Task<T> — Lazy Coroutine Return Type
// ═══════════════════════════════════════════════

/**
 * @brief Lazy coroutine return type for async operations
 * 
 * A Task<T> represents an asynchronous operation that produces a
 * value of type T. It is lazy — execution begins only when the
 * task is co_awaited or explicitly started.
 * 
 * Usage:
 *   Task<int> fetch_data() {
 *       auto result = co_await async_read(socket);
 *       co_return result.size();
 *   }
 *   
 *   Task<void> main_loop() {
 *       int n = co_await fetch_data();
 *   }
 */
template <typename T = void>
class Task;

// ─── Task<T> (non-void) ─────────────────────

template <typename T>
class Task {
public:
	struct promise_type {
		std::variant<std::monostate, T, std::exception_ptr> result_;
		std::coroutine_handle<> continuation_;

		Task get_return_object() noexcept {
			return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
		}

		std::suspend_always initial_suspend() noexcept { return {}; }

		struct FinalAwaiter {
			bool await_ready() noexcept { return false; }
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<promise_type> h) noexcept
			{
				if (h.promise().continuation_) {
					return h.promise().continuation_;
				}
				return std::noop_coroutine();
			}
			void await_resume() noexcept {}
		};

		FinalAwaiter final_suspend() noexcept { return {}; }

		void return_value(T value) {
			result_.template emplace<1>(std::move(value));
		}

		void unhandled_exception() {
			result_.template emplace<2>(std::current_exception());
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	Task() noexcept = default;
	explicit Task(handle_type h) noexcept : handle_(h) {}

	~Task() {
		if (handle_) handle_.destroy();
	}

	// Non-copyable, movable
	Task(const Task&) = delete;
	Task& operator=(const Task&) = delete;

	Task(Task&& other) noexcept : handle_(other.handle_) {
		other.handle_ = nullptr;
	}

	Task& operator=(Task&& other) noexcept {
		if (this != &other) {
			if (handle_) handle_.destroy();
			handle_ = other.handle_;
			other.handle_ = nullptr;
		}
		return *this;
	}

	// ─── Awaitable Interface ────────────

	bool await_ready() const noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
		handle_.promise().continuation_ = caller;
		return handle_;
	}

	T await_resume() {
		auto& result = handle_.promise().result_;
		if (std::holds_alternative<std::exception_ptr>(result)) {
			std::rethrow_exception(std::get<std::exception_ptr>(result));
		}
		return std::move(std::get<T>(result));
	}

	// ─── Synchronous Access ─────────────

	/**
	 * @brief Run the task synchronously and get the result
	 * 
	 * WARNING: This blocks the calling thread. Only use at
	 * the top level (e.g., main()).
	 */
	T run() {
		handle_.resume();
		while (!handle_.done()) {
			// Spin — in a real scheduler this would yield
		}
		return await_resume();
	}

	/**
	 * @brief Start the coroutine without waiting
	 */
	void start() {
		if (handle_ && !handle_.done()) {
			handle_.resume();
		}
	}

	/**
	 * @brief Check if the task has completed
	 */
	bool done() const noexcept {
		return handle_ && handle_.done();
	}

	handle_type handle() const noexcept { return handle_; }

private:
	handle_type handle_ = nullptr;
};

// ─── Task<void> specialization ──────────────

template <>
class Task<void> {
public:
	struct promise_type {
		std::exception_ptr exception_;
		std::coroutine_handle<> continuation_;

		Task get_return_object() noexcept {
			return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
		}

		std::suspend_always initial_suspend() noexcept { return {}; }

		struct FinalAwaiter {
			bool await_ready() noexcept { return false; }
			std::coroutine_handle<> await_suspend(
				std::coroutine_handle<promise_type> h) noexcept
			{
				if (h.promise().continuation_) {
					return h.promise().continuation_;
				}
				return std::noop_coroutine();
			}
			void await_resume() noexcept {}
		};

		FinalAwaiter final_suspend() noexcept { return {}; }

		void return_void() noexcept {}

		void unhandled_exception() {
			exception_ = std::current_exception();
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;

	Task() noexcept = default;
	explicit Task(handle_type h) noexcept : handle_(h) {}

	~Task() {
		if (handle_) handle_.destroy();
	}

	Task(const Task&) = delete;
	Task& operator=(const Task&) = delete;

	Task(Task&& other) noexcept : handle_(other.handle_) {
		other.handle_ = nullptr;
	}

	Task& operator=(Task&& other) noexcept {
		if (this != &other) {
			if (handle_) handle_.destroy();
			handle_ = other.handle_;
			other.handle_ = nullptr;
		}
		return *this;
	}

	bool await_ready() const noexcept { return false; }

	std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller) noexcept {
		handle_.promise().continuation_ = caller;
		return handle_;
	}

	void await_resume() {
		if (handle_.promise().exception_) {
			std::rethrow_exception(handle_.promise().exception_);
		}
	}

	void run() {
		handle_.resume();
		while (!handle_.done()) {}
		await_resume();
	}

	void start() {
		if (handle_ && !handle_.done()) {
			handle_.resume();
		}
	}

	bool done() const noexcept {
		return handle_ && handle_.done();
	}

	handle_type handle() const noexcept { return handle_; }

private:
	handle_type handle_ = nullptr;
};

// ═══════════════════════════════════════════════
//  Awaitable Helpers
// ═══════════════════════════════════════════════

/**
 * @brief Yield control to allow other coroutines to run
 */
struct Yield {
	bool await_ready() const noexcept { return false; }
	void await_suspend(std::coroutine_handle<> h) const noexcept {
		// Immediately resume — just yields the timeslice
		h.resume();
	}
	void await_resume() const noexcept {}
};

/**
 * @brief Create an immediately-ready awaitable from a value
 */
template <typename T>
struct ReadyAwaitable {
	T value_;

	explicit ReadyAwaitable(T val) : value_(std::move(val)) {}

	bool await_ready() const noexcept { return true; }
	void await_suspend(std::coroutine_handle<>) const noexcept {}
	T await_resume() { return std::move(value_); }
};

template <typename T>
ReadyAwaitable<T> ready(T value) {
	return ReadyAwaitable<T>(std::move(value));
}

/**
 * @brief Create a Task from a callback-based async operation
 * 
 * Bridges callback-style APIs with coroutines:
 *   auto result = co_await from_callback<int>([](auto resolve) {
 *       async_op([resolve](int val) { resolve(val); });
 *   });
 */
template <typename T>
struct CallbackAwaitable {
	using Resolver = std::function<void(T)>;
	using Initiator = std::function<void(Resolver)>;

	Initiator initiator_;
	T result_{};
	std::coroutine_handle<> continuation_;

	explicit CallbackAwaitable(Initiator init) : initiator_(std::move(init)) {}

	bool await_ready() const noexcept { return false; }

	void await_suspend(std::coroutine_handle<> h) {
		continuation_ = h;
		initiator_([this](T value) {
			result_ = std::move(value);
			if (continuation_) continuation_.resume();
		});
	}

	T await_resume() { return std::move(result_); }
};

template <typename T>
CallbackAwaitable<T> from_callback(typename CallbackAwaitable<T>::Initiator init) {
	return CallbackAwaitable<T>(std::move(init));
}

} // namespace async
} // namespace etherz

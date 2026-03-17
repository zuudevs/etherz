/**
 * @file thread_pool.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief General-purpose thread pool with task queue and futures
 * @version 4.0.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <type_traits>
#include <stdexcept>

namespace etherz {
namespace async {

// ═══════════════════════════════════════════════
//  Thread Pool
// ═══════════════════════════════════════════════

/**
 * @brief General-purpose thread pool with task submission and futures
 * 
 * Creates a fixed number of worker threads that process tasks from a
 * shared queue. Each submitted task returns a std::future for retrieving
 * the result.
 * 
 * Usage:
 *   ThreadPool pool(4);
 *   
 *   auto future = pool.submit([] {
 *       return expensive_computation();
 *   });
 *   
 *   auto result = future.get();
 *   
 *   // Pool shuts down gracefully on destruction
 */
class ThreadPool {
public:
	/**
	 * @brief Create a thread pool with the specified number of workers
	 * @param num_threads Number of worker threads (0 = hardware_concurrency)
	 */
	explicit ThreadPool(size_t num_threads = 0) {
		if (num_threads == 0) {
			num_threads = std::thread::hardware_concurrency();
			if (num_threads == 0) num_threads = 2;
		}

		for (size_t i = 0; i < num_threads; ++i) {
			workers_.emplace_back([this] { worker_loop(); });
		}
	}

	/**
	 * @brief Destroy the pool — waits for all pending tasks to complete
	 */
	~ThreadPool() noexcept {
		shutdown();
	}

	// Non-copyable, non-movable
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

	// ─── Task Submission ────────────────

	/**
	 * @brief Submit a callable task to the pool
	 * @tparam F Callable type
	 * @tparam Args Argument types
	 * @param f The callable
	 * @param args Arguments to pass to the callable
	 * @return std::future<return_type> Future for the task result
	 */
	template <typename F, typename... Args>
	auto submit(F&& f, Args&&... args)
		-> std::future<std::invoke_result_t<F, Args...>>
	{
		using return_type = std::invoke_result_t<F, Args...>;

		auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

		auto future = task->get_future();

		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (stopped_) {
				throw std::runtime_error("submit called on stopped ThreadPool");
			}
			tasks_.emplace([task]() { (*task)(); });
		}

		condition_.notify_one();
		return future;
	}

	// ─── Control ────────────────────────

	/**
	 * @brief Gracefully shut down the pool
	 * 
	 * Signals all workers to stop after completing their current task,
	 * then joins all threads. Remaining queued tasks are executed.
	 */
	void shutdown() noexcept {
		{
			std::unique_lock<std::mutex> lock(mutex_);
			if (stopped_) return;
			stopped_ = true;
		}
		condition_.notify_all();
		for (auto& worker : workers_) {
			if (worker.joinable()) {
				worker.join();
			}
		}
	}

	// ─── Queries ────────────────────────

	/**
	 * @brief Get the number of worker threads
	 */
	size_t thread_count() const noexcept { return workers_.size(); }

	/**
	 * @brief Get the number of pending tasks in the queue
	 */
	size_t pending_tasks() const noexcept {
		std::unique_lock<std::mutex> lock(mutex_);
		return tasks_.size();
	}

	/**
	 * @brief Check if the pool has been shut down
	 */
	bool is_stopped() const noexcept {
		std::unique_lock<std::mutex> lock(mutex_);
		return stopped_;
	}

private:
	std::vector<std::thread>            workers_;
	std::queue<std::function<void()>>   tasks_;
	mutable std::mutex                  mutex_;
	std::condition_variable             condition_;
	bool                                stopped_ = false;

	/**
	 * @brief Worker thread main loop
	 */
	void worker_loop() {
		while (true) {
			std::function<void()> task;

			{
				std::unique_lock<std::mutex> lock(mutex_);
				condition_.wait(lock, [this] {
					return stopped_ || !tasks_.empty();
				});

				if (stopped_ && tasks_.empty()) return;

				task = std::move(tasks_.front());
				tasks_.pop();
			}

			task();
		}
	}
};

} // namespace async
} // namespace etherz

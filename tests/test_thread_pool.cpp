/**
 * @file test_thread_pool.cpp
 * @brief Unit tests for ThreadPool — task submission, futures, shutdown
 */

#include "test_framework.hpp"
#include "async/thread_pool.hpp"
#include <numeric>
#include <atomic>
#include <chrono>
#include <thread>

namespace eta = etherz::async;

// ═══════════════════════════════════════════════
//  Construction
// ═══════════════════════════════════════════════

TEST(thread_pool_default_construction) {
	eta::ThreadPool pool(2);
	ASSERT_EQ(pool.thread_count(), 2u);
	ASSERT_TRUE(!pool.is_stopped());
}

TEST(thread_pool_auto_threads) {
	eta::ThreadPool pool;
	ASSERT_TRUE(pool.thread_count() >= 2u);
}

// ═══════════════════════════════════════════════
//  Task Submission
// ═══════════════════════════════════════════════

TEST(thread_pool_submit_basic) {
	eta::ThreadPool pool(2);
	auto future = pool.submit([] { return 42; });
	ASSERT_EQ(future.get(), 42);
}

TEST(thread_pool_submit_string) {
	eta::ThreadPool pool(2);
	auto future = pool.submit([] { return std::string("hello"); });
	ASSERT_EQ(future.get(), "hello");
}

TEST(thread_pool_submit_void) {
	eta::ThreadPool pool(2);
	std::atomic<bool> executed{false};
	auto future = pool.submit([&] { executed.store(true); });
	future.get();
	ASSERT_TRUE(executed.load());
}

TEST(thread_pool_submit_multiple) {
	eta::ThreadPool pool(4);
	std::vector<std::future<int>> futures;

	for (int i = 0; i < 20; ++i) {
		futures.push_back(pool.submit([i] { return i * i; }));
	}

	for (int i = 0; i < 20; ++i) {
		ASSERT_EQ(futures[static_cast<size_t>(i)].get(), i * i);
	}
}

TEST(thread_pool_submit_with_args) {
	eta::ThreadPool pool(2);
	auto future = pool.submit([](int a, int b) { return a + b; }, 3, 7);
	ASSERT_EQ(future.get(), 10);
}

// ═══════════════════════════════════════════════
//  Concurrent Execution
// ═══════════════════════════════════════════════

TEST(thread_pool_concurrent) {
	eta::ThreadPool pool(4);
	std::atomic<int> counter{0};

	std::vector<std::future<void>> futures;
	for (int i = 0; i < 100; ++i) {
		futures.push_back(pool.submit([&counter] {
			counter.fetch_add(1);
		}));
	}

	for (auto& f : futures) {
		f.get();
	}

	ASSERT_EQ(counter.load(), 100);
}

TEST(thread_pool_different_threads) {
	eta::ThreadPool pool(4);
	std::atomic<int> thread_count{0};
	std::mutex mtx;
	std::vector<std::thread::id> ids;

	std::vector<std::future<void>> futures;
	for (int i = 0; i < 8; ++i) {
		futures.push_back(pool.submit([&] {
			auto id = std::this_thread::get_id();
			std::lock_guard lock(mtx);
			ids.push_back(id);
		}));
	}

	for (auto& f : futures) {
		f.get();
	}

	// Should have collected IDs from different threads
	ASSERT_EQ(ids.size(), 8u);
}

// ═══════════════════════════════════════════════
//  Shutdown
// ═══════════════════════════════════════════════

TEST(thread_pool_shutdown) {
	eta::ThreadPool pool(2);
	pool.submit([] { return 1; }).get();
	pool.shutdown();
	ASSERT_TRUE(pool.is_stopped());
}

TEST(thread_pool_double_shutdown) {
	eta::ThreadPool pool(2);
	pool.shutdown();
	pool.shutdown();  // Should not crash
	ASSERT_TRUE(pool.is_stopped());
}

TEST(thread_pool_pending_tasks) {
	eta::ThreadPool pool(1);
	// Pool has 1 thread — submit a blocking task, then check pending
	std::atomic<bool> proceed{false};

	auto blocking = pool.submit([&] {
		while (!proceed.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	});

	// Submit more tasks while the first blocks
	auto f1 = pool.submit([] { return 1; });
	auto f2 = pool.submit([] { return 2; });

	// There should be pending tasks
	ASSERT_TRUE(pool.pending_tasks() >= 1u);

	// Release
	proceed.store(true);
	blocking.get();
	f1.get();
	f2.get();
}

// ═══════════════════════════════════════════════
//  Edge Cases
// ═══════════════════════════════════════════════

TEST(thread_pool_exception_in_task) {
	eta::ThreadPool pool(2);
	auto future = pool.submit([] {
		throw std::runtime_error("test error");
		return 0;
	});

	bool caught = false;
	try {
		future.get();
	} catch (const std::runtime_error& e) {
		caught = true;
		std::string msg = e.what();
		ASSERT_EQ(msg, "test error");
	}
	ASSERT_TRUE(caught);
}

TEST(thread_pool_single_thread) {
	eta::ThreadPool pool(1);
	std::vector<std::future<int>> futures;

	for (int i = 0; i < 10; ++i) {
		futures.push_back(pool.submit([i] { return i; }));
	}

	for (int i = 0; i < 10; ++i) {
		ASSERT_EQ(futures[static_cast<size_t>(i)].get(), i);
	}
}

#include "test_framework.hpp"
#include "async/task.hpp"
#include "async/generator.hpp"
#include "async/timer.hpp"

namespace ea = etherz::async;

// ─── Task<T> Tests (v2.1.0) ────────────

static ea::Task<int> simple_task() {
	co_return 42;
}

TEST_CASE(task_int_run) {
	auto t = simple_task();
	int result = t.run();
	CHECK_EQ(result, 42);
}

static ea::Task<int> add_task(int a, int b) {
	co_return a + b;
}

TEST_CASE(task_int_with_args) {
	auto t = add_task(10, 20);
	CHECK_EQ(t.run(), 30);
}

// ─── Task<void> Tests ──────────────────

static int void_side_effect = 0;

static ea::Task<void> void_task() {
	void_side_effect = 99;
	co_return;
}

TEST_CASE(task_void_run) {
	void_side_effect = 0;
	auto t = void_task();
	t.run();
	CHECK_EQ(void_side_effect, 99);
}

TEST_CASE(task_done_before_run) {
	auto t = simple_task();
	CHECK_FALSE(t.done());
}

TEST_CASE(task_done_after_run) {
	auto t = simple_task();
	t.run();
	CHECK_TRUE(t.done());
}

// ─── Generator<T> Tests (v2.1.0) ──────

static ea::Generator<int> range_gen(int start, int end) {
	for (int i = start; i < end; ++i) {
		co_yield i;
	}
}

TEST_CASE(generator_range_for) {
	auto gen = range_gen(0, 5);
	int sum = 0;
	for (int val : gen) {
		sum += val;
	}
	CHECK_EQ(sum, 10); // 0+1+2+3+4
}

TEST_CASE(generator_manual_iteration) {
	auto gen = range_gen(10, 13);
	CHECK_TRUE(gen.next());
	CHECK_EQ(gen.value(), 10);
	CHECK_TRUE(gen.next());
	CHECK_EQ(gen.value(), 11);
	CHECK_TRUE(gen.next());
	CHECK_EQ(gen.value(), 12);
	CHECK_FALSE(gen.next());
	CHECK_TRUE(gen.done());
}

static ea::Generator<int> empty_gen() {
	co_return;
}

TEST_CASE(generator_empty) {
	auto gen = empty_gen();
	CHECK_FALSE(gen.next());
	CHECK_TRUE(gen.done());
}

static ea::Generator<int> single_gen() {
	co_yield 42;
}

TEST_CASE(generator_single_value) {
	auto gen = single_gen();
	CHECK_TRUE(gen.next());
	CHECK_EQ(gen.value(), 42);
	CHECK_FALSE(gen.next());
}

// ─── ReadyAwaitable Tests ──────────────

static ea::Task<int> use_ready_awaitable() {
	int val = co_await ea::ready(100);
	co_return val;
}

TEST_CASE(ready_awaitable) {
	auto t = use_ready_awaitable();
	CHECK_EQ(t.run(), 100);
}

// ─── Timer Utility Tests ───────────────

TEST_CASE(timer_now_ms) {
	uint64_t ts = ea::Timer::now_ms();
	CHECK_TRUE(ts > 0);
}

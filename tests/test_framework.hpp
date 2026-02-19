/**
 * @file test_framework.hpp
 * @brief Minimal unit test framework for Etherz
 * @version 1.0.0
 *
 * Lightweight, header-only test macros. No external dependencies.
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <print>
#include <source_location>

namespace etherz_test {

struct TestCase {
	std::string name;
	std::function<void()> fn;
};

struct TestResult {
	int passed = 0;
	int failed = 0;
	std::vector<std::string> failures;
};

inline std::vector<TestCase>& registry() {
	static std::vector<TestCase> cases;
	return cases;
}

inline TestResult& result() {
	static TestResult r;
	return r;
}

struct Registrar {
	Registrar(const char* name, std::function<void()> fn) {
		registry().push_back({name, std::move(fn)});
	}
};

inline void check_impl(bool cond, const char* expr,
	std::source_location loc = std::source_location::current()) {
	if (cond) {
		result().passed++;
	} else {
		result().failed++;
		auto msg = std::format("  FAIL: {} ({}:{})",
			expr, loc.file_name(), loc.line());
		result().failures.push_back(msg);
		std::print("{}\n", msg);
	}
}

inline int run_all() {
	auto& cases = registry();
	auto& res = result();

	std::print("═══════════════════════════════════\n");
	std::print("  Etherz Test Suite\n");
	std::print("═══════════════════════════════════\n\n");

	for (auto& tc : cases) {
		std::print("── {} ──\n", tc.name);
		tc.fn();
		std::print("\n");
	}

	std::print("═══════════════════════════════════\n");
	std::print("  Results: {} passed, {} failed\n", res.passed, res.failed);
	std::print("═══════════════════════════════════\n");

	if (!res.failures.empty()) {
		std::print("\nFailures:\n");
		for (auto& f : res.failures) std::print("{}\n", f);
	}

	return res.failed > 0 ? 1 : 0;
}

} // namespace etherz_test

#define TEST_CASE(name) \
	static void test_##name(); \
	static etherz_test::Registrar reg_##name(#name, test_##name); \
	static void test_##name()

#define CHECK(expr) etherz_test::check_impl((expr), #expr)
#define CHECK_EQ(a, b) etherz_test::check_impl((a) == (b), #a " == " #b)
#define CHECK_NE(a, b) etherz_test::check_impl((a) != (b), #a " != " #b)
#define CHECK_TRUE(expr) etherz_test::check_impl((expr), #expr " is true")
#define CHECK_FALSE(expr) etherz_test::check_impl(!(expr), #expr " is false")

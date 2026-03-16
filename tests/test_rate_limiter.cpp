#include "test_framework.hpp"
#include "net/rate_limiter.hpp"
#include "net/bandwidth_monitor.hpp"

namespace en = etherz::net;

// ─── RateLimiter Tests (v1.3.0) ────────

TEST_CASE(rate_limiter_construction) {
	en::RateLimiter limiter(1000.0, 500.0);
	CHECK_EQ(limiter.rate(), 1000.0);
	CHECK_EQ(limiter.burst(), 500.0);
}

TEST_CASE(rate_limiter_try_consume) {
	en::RateLimiter limiter(10000.0, 100.0);
	CHECK_TRUE(limiter.try_consume(50.0));
	CHECK_TRUE(limiter.try_consume(50.0));
	// Bucket should be empty now
	CHECK_FALSE(limiter.try_consume(50.0));
}

TEST_CASE(rate_limiter_available) {
	en::RateLimiter limiter(100.0, 100.0);
	double avail = limiter.available();
	CHECK_TRUE(avail > 99.0);  // Should be ~100 (burst)
}

TEST_CASE(rate_limiter_reset) {
	en::RateLimiter limiter(100.0, 100.0);
	limiter.try_consume(100.0);
	limiter.reset();
	CHECK_TRUE(limiter.available() > 99.0);
}

TEST_CASE(rate_limiter_set_rate) {
	en::RateLimiter limiter(100.0, 100.0);
	limiter.set_rate(200.0);
	CHECK_EQ(limiter.rate(), 200.0);
}

TEST_CASE(rate_limiter_set_burst) {
	en::RateLimiter limiter(100.0, 100.0);
	limiter.set_burst(50.0);
	CHECK_EQ(limiter.burst(), 50.0);
	// Available should be clamped to new burst
	CHECK_TRUE(limiter.available() <= 50.0);
}

TEST_CASE(rate_limiter_wait_time) {
	en::RateLimiter limiter(100.0, 100.0);
	CHECK_EQ(limiter.wait_time_ms(50.0), static_cast<uint32_t>(0));
	limiter.try_consume(100.0);
	CHECK_TRUE(limiter.wait_time_ms(50.0) > 0);
}

// ─── BandwidthMonitor Tests (v1.3.0) ───

TEST_CASE(bandwidth_monitor_construction) {
	en::BandwidthMonitor bw;
	CHECK_EQ(bw.total_sent(), static_cast<size_t>(0));
	CHECK_EQ(bw.total_received(), static_cast<size_t>(0));
	CHECK_EQ(bw.total_bytes(), static_cast<size_t>(0));
}

TEST_CASE(bandwidth_monitor_record) {
	en::BandwidthMonitor bw;
	bw.record_sent(1024);
	bw.record_received(2048);
	CHECK_EQ(bw.total_sent(), static_cast<size_t>(1024));
	CHECK_EQ(bw.total_received(), static_cast<size_t>(2048));
	CHECK_EQ(bw.total_bytes(), static_cast<size_t>(3072));
}

TEST_CASE(bandwidth_monitor_format_bytes) {
	CHECK_EQ(en::BandwidthMonitor::format_bytes(500), std::string("500 B"));
	CHECK_EQ(en::BandwidthMonitor::format_bytes(1024), std::string("1.0 KB"));
	CHECK_EQ(en::BandwidthMonitor::format_bytes(1048576), std::string("1.0 MB"));
	CHECK_EQ(en::BandwidthMonitor::format_bytes(1073741824), std::string("1.0 GB"));
}

TEST_CASE(bandwidth_monitor_format_rate) {
	auto result = en::BandwidthMonitor::format_rate(1024.0);
	CHECK_EQ(result, std::string("1.0 KB/s"));
}

TEST_CASE(bandwidth_monitor_reset) {
	en::BandwidthMonitor bw;
	bw.record_sent(1000);
	bw.record_received(2000);
	bw.reset();
	CHECK_EQ(bw.total_sent(), static_cast<size_t>(0));
	CHECK_EQ(bw.total_received(), static_cast<size_t>(0));
}

TEST_CASE(bandwidth_monitor_elapsed) {
	en::BandwidthMonitor bw;
	CHECK_TRUE(bw.elapsed_seconds() >= 0.0);
}

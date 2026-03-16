#include "test_framework.hpp"
#include "core/error.hpp"

namespace ec = etherz::core;

// ─── v1.1.0 Error Codes ────────────────

TEST_CASE(error_pool_exhausted) {
	CHECK_EQ(ec::error_message(ec::Error::PoolExhausted),
		std::string_view("Connection pool exhausted"));
	CHECK_TRUE(ec::is_error(ec::Error::PoolExhausted));
	CHECK_FALSE(ec::is_ok(ec::Error::PoolExhausted));
}

TEST_CASE(error_multicast_error) {
	CHECK_EQ(ec::error_message(ec::Error::MulticastError),
		std::string_view("Multicast operation failed"));
}

TEST_CASE(error_proxy_error) {
	CHECK_EQ(ec::error_message(ec::Error::ProxyError),
		std::string_view("Proxy connection failed"));
}

TEST_CASE(error_proxy_auth_failed) {
	CHECK_EQ(ec::error_message(ec::Error::ProxyAuthFailed),
		std::string_view("Proxy authentication failed"));
}

TEST_CASE(error_rate_limited) {
	CHECK_EQ(ec::error_message(ec::Error::RateLimited),
		std::string_view("Rate limit exceeded"));
}

TEST_CASE(error_none_is_ok) {
	CHECK_TRUE(ec::is_ok(ec::Error::None));
	CHECK_FALSE(ec::is_error(ec::Error::None));
}

TEST_CASE(error_from_platform_zero) {
	CHECK_EQ(ec::from_platform_error(0), ec::Error::None);
}

#include "test_framework.hpp"
#include "net/internet_protocol.hpp"

// ─── IPv4 Tests ─────────────────

TEST_CASE(ipv4_default_constructor) {
	etherz::net::Ip<4> ip;
	auto b = ip.bytes();
	CHECK_EQ(b[0], 0);
	CHECK_EQ(b[1], 0);
	CHECK_EQ(b[2], 0);
	CHECK_EQ(b[3], 0);
}

TEST_CASE(ipv4_octet_constructor) {
	auto ip = etherz::net::Ip<4>(192, 168, 1, 100);
	auto b = ip.bytes();
	CHECK_EQ(b[0], 192);
	CHECK_EQ(b[1], 168);
	CHECK_EQ(b[2], 1);
	CHECK_EQ(b[3], 100);
}

TEST_CASE(ipv4_string_parse) {
	auto ip = etherz::net::Ip<4>("10.0.0.1");
	auto b = ip.bytes();
	CHECK_EQ(b[0], 10);
	CHECK_EQ(b[1], 0);
	CHECK_EQ(b[2], 0);
	CHECK_EQ(b[3], 1);
}

TEST_CASE(ipv4_to_uint32) {
	auto ip = etherz::net::Ip<4>(192, 168, 1, 1);
	uint32_t val = ip.to_uint32();
	CHECK_EQ(val, 0xC0A80101u);
}

TEST_CASE(ipv4_arithmetic) {
	auto ip = etherz::net::Ip<4>(192, 168, 1, 1);
	auto next = ip + 1;
	auto b = next.bytes();
	CHECK_EQ(b[3], 2);
}

TEST_CASE(ipv4_comparison) {
	auto a = etherz::net::Ip<4>(192, 168, 1, 1);
	auto b = etherz::net::Ip<4>(192, 168, 1, 2);
	auto c = etherz::net::Ip<4>(192, 168, 1, 1);
	CHECK_TRUE(a < b);
	CHECK_TRUE(a == c);
	CHECK_TRUE(a != b);
}

// ─── IPv6 Tests ─────────────────

TEST_CASE(ipv6_default_constructor) {
	etherz::net::Ip<6> ip;
	auto b = ip.bytes();
	for (int i = 0; i < 8; ++i)
		CHECK_EQ(b[i], 0);
}

TEST_CASE(ipv6_group_constructor) {
	auto ip = etherz::net::Ip<6>(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1);
	auto b = ip.bytes();
	CHECK_EQ(b[0], 0x2001);
	CHECK_EQ(b[1], 0x0db8);
	CHECK_EQ(b[7], 1);
}

TEST_CASE(ipv6_comparison) {
	auto a = etherz::net::Ip<6>(0, 0, 0, 0, 0, 0, 0, 1);
	auto b = etherz::net::Ip<6>(0, 0, 0, 0, 0, 0, 0, 2);
	CHECK_TRUE(a < b);
	CHECK_TRUE(a != b);
}

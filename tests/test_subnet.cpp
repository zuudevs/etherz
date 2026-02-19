#include "test_framework.hpp"
#include "net/subnet.hpp"

TEST_CASE(subnet_parse_cidr) {
	auto s = etherz::net::Subnet<etherz::net::Ip<4>>::parse("192.168.1.0/24");
	CHECK_EQ(s.prefix_length(), 24);
	auto net = s.network().bytes();
	CHECK_EQ(net[0], 192);
	CHECK_EQ(net[1], 168);
	CHECK_EQ(net[2], 1);
	CHECK_EQ(net[3], 0);
}

TEST_CASE(subnet_mask) {
	auto s = etherz::net::Subnet<etherz::net::Ip<4>>::parse("10.0.0.0/8");
	auto m = s.mask().bytes();
	CHECK_EQ(m[0], 255);
	CHECK_EQ(m[1], 0);
	CHECK_EQ(m[2], 0);
	CHECK_EQ(m[3], 0);
}

TEST_CASE(subnet_contains) {
	auto s = etherz::net::Subnet<etherz::net::Ip<4>>::parse("192.168.1.0/24");
	CHECK_TRUE(s.contains(etherz::net::Ip<4>(192, 168, 1, 100)));
	CHECK_TRUE(s.contains(etherz::net::Ip<4>(192, 168, 1, 1)));
	CHECK_FALSE(s.contains(etherz::net::Ip<4>(10, 0, 0, 1)));
	CHECK_FALSE(s.contains(etherz::net::Ip<4>(192, 168, 2, 1)));
}

TEST_CASE(subnet_broadcast) {
	auto s = etherz::net::Subnet<etherz::net::Ip<4>>::parse("192.168.1.0/24");
	auto bc = s.broadcast().bytes();
	CHECK_EQ(bc[0], 192);
	CHECK_EQ(bc[1], 168);
	CHECK_EQ(bc[2], 1);
	CHECK_EQ(bc[3], 255);
}

TEST_CASE(subnet_host_count) {
	auto s24 = etherz::net::Subnet<etherz::net::Ip<4>>::parse("10.0.0.0/24");
	CHECK_EQ(s24.host_count(), 254u);

	auto s16 = etherz::net::Subnet<etherz::net::Ip<4>>::parse("172.16.0.0/16");
	CHECK_EQ(s16.host_count(), 65534u);

	auto s32 = etherz::net::Subnet<etherz::net::Ip<4>>::parse("1.2.3.4/32");
	CHECK_EQ(s32.host_count(), 1u);
}

TEST_CASE(subnet_to_string) {
	auto s = etherz::net::Subnet<etherz::net::Ip<4>>::parse("10.20.30.0/24");
	CHECK_EQ(s.to_string(), std::string("10.20.30.0/24"));
}

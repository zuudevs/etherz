/**
 * @file test_stun.cpp
 * @brief Unit tests for STUN — packet construction, response parsing, NAT type
 */

#include "test_framework.hpp"
#include "net/stun.hpp"
#include <cstring>

namespace etn = etherz::net;

// ═══════════════════════════════════════════════
//  STUN Binding Request Construction
// ═══════════════════════════════════════════════

TEST(stun_binding_request_size) {
	auto pkt = etn::StunClient::build_request();
	// STUN header is exactly 20 bytes (no attributes)
	ASSERT_EQ(pkt.size(), 20u);
}

TEST(stun_binding_request_type) {
	auto pkt = etn::StunClient::build_request();
	// Message type: 0x0001 (Binding Request)
	ASSERT_EQ(pkt[0], 0x00);
	ASSERT_EQ(pkt[1], 0x01);
}

TEST(stun_binding_request_cookie) {
	auto pkt = etn::StunClient::build_request();
	// Magic Cookie: 0x2112A442
	ASSERT_EQ(pkt[4], 0x21);
	ASSERT_EQ(pkt[5], 0x12);
	ASSERT_EQ(pkt[6], 0xA4);
	ASSERT_EQ(pkt[7], 0x42);
}

TEST(stun_binding_request_length) {
	auto pkt = etn::StunClient::build_request();
	// Message length: 0 (no attributes)
	ASSERT_EQ(pkt[2], 0x00);
	ASSERT_EQ(pkt[3], 0x00);
}

TEST(stun_binding_request_with_txn_id) {
	std::array<uint8_t, 12> txn = {1,2,3,4,5,6,7,8,9,10,11,12};
	auto pkt = etn::StunClient::build_request(txn);

	// Verify transaction ID is in bytes 8-19
	for (int i = 0; i < 12; ++i) {
		ASSERT_EQ(pkt[8 + i], txn[static_cast<size_t>(i)]);
	}
}

// ═══════════════════════════════════════════════
//  STUN Response Parsing — XOR-MAPPED-ADDRESS
// ═══════════════════════════════════════════════

TEST(stun_parse_xor_mapped_address) {
	std::array<uint8_t, 12> txn = {0xAA, 0xBB, 0xCC, 0xDD,
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};

	// Build a fake binding response with XOR-MAPPED-ADDRESS
	// Public IP: 203.0.113.1 (0xCB007101)
	// Public Port: 54321
	uint32_t real_ip = (203u << 24) | (0u << 16) | (113u << 8) | 1u;
	uint32_t magic = 0x2112A442;
	uint32_t xor_ip = real_ip ^ magic;
	uint16_t xor_port = 54321 ^ static_cast<uint16_t>((magic >> 16) & 0xFFFF);

	std::vector<uint8_t> response;
	// Header
	response.push_back(0x01); response.push_back(0x01);  // Binding Response
	response.push_back(0x00); response.push_back(0x0C);  // Length: 12
	// Magic Cookie
	response.push_back(0x21); response.push_back(0x12);
	response.push_back(0xA4); response.push_back(0x42);
	// Transaction ID
	response.insert(response.end(), txn.begin(), txn.end());

	// XOR-MAPPED-ADDRESS attribute
	response.push_back(0x00); response.push_back(0x20);  // Type
	response.push_back(0x00); response.push_back(0x08);  // Length: 8
	response.push_back(0x00);                              // Reserved
	response.push_back(0x01);                              // Family: IPv4
	response.push_back(static_cast<uint8_t>((xor_port >> 8) & 0xFF));
	response.push_back(static_cast<uint8_t>(xor_port & 0xFF));
	response.push_back(static_cast<uint8_t>((xor_ip >> 24) & 0xFF));
	response.push_back(static_cast<uint8_t>((xor_ip >> 16) & 0xFF));
	response.push_back(static_cast<uint8_t>((xor_ip >> 8) & 0xFF));
	response.push_back(static_cast<uint8_t>(xor_ip & 0xFF));

	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(response.data(), response.size()), txn);

	ASSERT_TRUE(result.valid);
	ASSERT_EQ(result.port, 54321);
	ASSERT_EQ(result.ip.to_uint32(), real_ip);
}

// ═══════════════════════════════════════════════
//  STUN Response Parsing — MAPPED-ADDRESS
// ═══════════════════════════════════════════════

TEST(stun_parse_mapped_address) {
	std::array<uint8_t, 12> txn = {};
	std::memset(txn.data(), 0x42, 12);

	std::vector<uint8_t> response;
	// Header
	response.push_back(0x01); response.push_back(0x01);
	response.push_back(0x00); response.push_back(0x0C);  // Length: 12
	response.push_back(0x21); response.push_back(0x12);
	response.push_back(0xA4); response.push_back(0x42);
	response.insert(response.end(), txn.begin(), txn.end());

	// MAPPED-ADDRESS attribute (0x0001)
	response.push_back(0x00); response.push_back(0x01);  // Type
	response.push_back(0x00); response.push_back(0x08);  // Length: 8
	response.push_back(0x00);                              // Reserved
	response.push_back(0x01);                              // Family: IPv4
	response.push_back(0x1F); response.push_back(0x90);  // Port: 8080
	response.push_back(198);  response.push_back(51);     // IP: 198.51.100.42
	response.push_back(100);  response.push_back(42);

	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(response.data(), response.size()), txn);

	ASSERT_TRUE(result.valid);
	ASSERT_EQ(result.port, 8080);
}

// ═══════════════════════════════════════════════
//  Invalid Responses
// ═══════════════════════════════════════════════

TEST(stun_parse_too_short) {
	std::array<uint8_t, 12> txn = {};
	std::array<uint8_t, 10> data = {};
	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(data.data(), data.size()), txn);
	ASSERT_TRUE(!result.valid);
}

TEST(stun_parse_wrong_type) {
	std::array<uint8_t, 12> txn = {};

	std::vector<uint8_t> response(20, 0);
	response[0] = 0x00; response[1] = 0x01;  // Binding Request (wrong)
	response[4] = 0x21; response[5] = 0x12;
	response[6] = 0xA4; response[7] = 0x42;

	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(response.data(), response.size()), txn);
	ASSERT_TRUE(!result.valid);
}

TEST(stun_parse_wrong_cookie) {
	std::array<uint8_t, 12> txn = {};

	std::vector<uint8_t> response(20, 0);
	response[0] = 0x01; response[1] = 0x01;  // Binding Response
	response[4] = 0xFF; response[5] = 0xFF;  // Wrong cookie
	response[6] = 0xFF; response[7] = 0xFF;

	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(response.data(), response.size()), txn);
	ASSERT_TRUE(!result.valid);
}

TEST(stun_parse_wrong_txn_id) {
	std::array<uint8_t, 12> txn = {};
	std::memset(txn.data(), 0xAA, 12);

	std::vector<uint8_t> response(20, 0);
	response[0] = 0x01; response[1] = 0x01;
	response[4] = 0x21; response[5] = 0x12;
	response[6] = 0xA4; response[7] = 0x42;
	// Transaction ID is all zeros (mismatch)

	auto result = etn::StunClient::parse_response(
		std::span<const uint8_t>(response.data(), response.size()), txn);
	ASSERT_TRUE(!result.valid);
}

// ═══════════════════════════════════════════════
//  NAT Type Strings
// ═══════════════════════════════════════════════

TEST(stun_nat_type_strings) {
	ASSERT_EQ(etn::nat_type_string(etn::NatType::Open), "Open (No NAT)");
	ASSERT_EQ(etn::nat_type_string(etn::NatType::FullCone), "Full Cone NAT");
	ASSERT_EQ(etn::nat_type_string(etn::NatType::Restricted), "Restricted Cone NAT");
	ASSERT_EQ(etn::nat_type_string(etn::NatType::PortRestricted), "Port Restricted Cone NAT");
	ASSERT_EQ(etn::nat_type_string(etn::NatType::Symmetric), "Symmetric NAT");
	ASSERT_EQ(etn::nat_type_string(etn::NatType::Unknown), "Unknown");
}

// ═══════════════════════════════════════════════
//  StunResult Display
// ═══════════════════════════════════════════════

TEST(stun_result_display) {
	etn::StunResult r;
	r.public_ip   = etn::Ip<4>(203, 0, 113, 1);
	r.public_port = 54321;
	r.nat_type    = etn::NatType::FullCone;
	r.success     = true;

	auto disp = r.display();
	ASSERT_TRUE(disp.find("203.0.113.1") != std::string::npos);
	ASSERT_TRUE(disp.find("54321") != std::string::npos);
	ASSERT_TRUE(disp.find("Full Cone") != std::string::npos);
}

// ═══════════════════════════════════════════════
//  Transaction ID Generation
// ═══════════════════════════════════════════════

TEST(stun_transaction_id_unique) {
	auto id1 = etn::stun_detail::generate_transaction_id();
	auto id2 = etn::stun_detail::generate_transaction_id();
	// Very unlikely to be the same
	bool same = std::memcmp(id1.data(), id2.data(), 12) == 0;
	ASSERT_TRUE(!same);
}

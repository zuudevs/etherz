#include "test_framework.hpp"
#include "net/mdns.hpp"

namespace en = etherz::net;

// ─── DNS Name Encoding (v2.5.0) ──────

TEST_CASE(dns_encode_name_simple) {
	auto encoded = en::dns_encode_name("myhost.local");
	// "\x06myhost\x05local\x00"
	CHECK_EQ(encoded[0], static_cast<uint8_t>(6)); // "myhost" = 6 chars
	CHECK_EQ(encoded[1], static_cast<uint8_t>('m'));
	CHECK_EQ(encoded[7], static_cast<uint8_t>(5)); // "local" = 5 chars
	CHECK_EQ(encoded[8], static_cast<uint8_t>('l'));
	CHECK_EQ(encoded[13], static_cast<uint8_t>(0)); // root label
}

TEST_CASE(dns_encode_decode_roundtrip) {
	auto encoded = en::dns_encode_name("my.device.local");
	size_t pos = 0;
	auto decoded = en::dns_decode_name(encoded, pos);
	CHECK_EQ(decoded, std::string("my.device.local"));
}

TEST_CASE(dns_encode_single_label) {
	auto encoded = en::dns_encode_name("hostname");
	size_t pos = 0;
	auto decoded = en::dns_decode_name(encoded, pos);
	CHECK_EQ(decoded, std::string("hostname"));
}

// ─── DnsRecord ────────────────────────

TEST_CASE(dns_record_ipv4_roundtrip) {
	en::DnsRecord record;
	en::Ip<4> ip(192, 168, 1, 42);
	record.set_ipv4(ip);

	CHECK_EQ(record.type, en::DnsRecordType::A);
	CHECK_EQ(record.rdata.size(), static_cast<size_t>(4));

	auto extracted = record.get_ipv4();
	CHECK_EQ(extracted.bytes()[0], static_cast<uint8_t>(192));
	CHECK_EQ(extracted.bytes()[1], static_cast<uint8_t>(168));
	CHECK_EQ(extracted.bytes()[2], static_cast<uint8_t>(1));
	CHECK_EQ(extracted.bytes()[3], static_cast<uint8_t>(42));
}

TEST_CASE(dns_record_default_ttl) {
	en::DnsRecord record;
	CHECK_EQ(record.ttl, static_cast<uint32_t>(120));
	CHECK_EQ(record.class_, static_cast<uint16_t>(1));
}

// ─── mDNS Constants ──────────────────

TEST_CASE(mdns_constants) {
	CHECK_EQ(en::mdns::PORT, static_cast<uint16_t>(5353));
	CHECK_EQ(en::mdns::LOCAL_DOMAIN, std::string_view(".local"));
}

// ─── DnsRecordType enum values ───────

TEST_CASE(dns_record_type_values) {
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::A), static_cast<uint16_t>(1));
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::AAAA), static_cast<uint16_t>(28));
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::PTR), static_cast<uint16_t>(12));
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::SRV), static_cast<uint16_t>(33));
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::TXT), static_cast<uint16_t>(16));
	CHECK_EQ(static_cast<uint16_t>(en::DnsRecordType::ANY), static_cast<uint16_t>(255));
}

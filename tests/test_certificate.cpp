#include "test_framework.hpp"
#include "security/certificate.hpp"

namespace ets = etherz::security;

TEST_CASE(cert_self_signed) {
	auto cert = ets::make_self_signed_info("test.local");
	CHECK_TRUE(cert.valid());
	CHECK_EQ(cert.subject, std::string("CN=test.local"));
	CHECK_EQ(cert.issuer, cert.subject); // Self-signed
	CHECK_EQ(cert.key_bits, 2048);
	CHECK_FALSE(cert.serial.empty());
	CHECK_FALSE(cert.fingerprint.empty());
}

TEST_CASE(cert_custom_key_bits) {
	auto cert = ets::make_self_signed_info("ec.local", 256);
	CHECK_EQ(cert.key_bits, 256);
}

TEST_CASE(cert_empty_invalid) {
	ets::CertInfo empty;
	CHECK_FALSE(empty.valid());
	CHECK_TRUE(empty.subject.empty());
}

#include "test_framework.hpp"
#include "security/dtls_context.hpp"

namespace es = etherz::security;

// ─── DTLS Version Names (v2.2.0) ──────

TEST_CASE(dtls_version_name_1_0) {
	CHECK_EQ(es::dtls_version_name(es::DtlsVersion::Dtls_1_0),
		std::string_view("DTLS 1.0"));
}

TEST_CASE(dtls_version_name_1_2) {
	CHECK_EQ(es::dtls_version_name(es::DtlsVersion::Dtls_1_2),
		std::string_view("DTLS 1.2"));
}

TEST_CASE(dtls_version_name_1_3) {
	CHECK_EQ(es::dtls_version_name(es::DtlsVersion::Dtls_1_3),
		std::string_view("DTLS 1.3"));
}

// ─── DtlsConfig Factory Methods ──────

TEST_CASE(dtls_config_server) {
	auto cfg = es::DtlsConfig::server("cert.pem", "key.pem");
	CHECK_EQ(cfg.cert_file, std::string("cert.pem"));
	CHECK_EQ(cfg.key_file, std::string("key.pem"));
}

TEST_CASE(dtls_config_client) {
	auto cfg = es::DtlsConfig::client("ca.pem");
	CHECK_EQ(cfg.ca_file, std::string("ca.pem"));
	CHECK_TRUE(cfg.verify_peer);
}

TEST_CASE(dtls_config_client_no_ca) {
	auto cfg = es::DtlsConfig::client();
	CHECK_TRUE(cfg.ca_file.empty());
	CHECK_FALSE(cfg.verify_peer);
}

TEST_CASE(dtls_config_defaults) {
	es::DtlsConfig cfg;
	CHECK_EQ(cfg.min_version, es::DtlsVersion::Dtls_1_2);
	CHECK_EQ(cfg.max_version, es::DtlsVersion::Dtls_1_3);
	CHECK_EQ(cfg.mtu, static_cast<uint32_t>(1400));
	CHECK_EQ(cfg.retransmit_ms, static_cast<uint32_t>(1000));
	CHECK_TRUE(cfg.verify_peer);
	CHECK_FALSE(cfg.allow_self_signed);
}

// ─── DtlsContext ──────────────────────

TEST_CASE(dtls_context_construction) {
	es::DtlsContext ctx;
	CHECK_FALSE(ctx.is_initialized());
}

TEST_CASE(dtls_context_initialize) {
	es::DtlsContext ctx;
	auto err = ctx.initialize();
	// On Windows, should succeed
	CHECK_TRUE(etherz::core::is_ok(err));
	CHECK_TRUE(ctx.is_initialized());
}

TEST_CASE(dtls_context_cookie_secret) {
	es::DtlsContext ctx;
	ctx.set_cookie_secret("my_secret_key");
	CHECK_EQ(ctx.cookie_secret(), std::string("my_secret_key"));
}

TEST_CASE(dtls_context_config_access) {
	auto cfg = es::DtlsConfig::server("cert.pem", "key.pem");
	es::DtlsContext ctx(std::move(cfg));
	CHECK_EQ(ctx.config().cert_file, std::string("cert.pem"));
}

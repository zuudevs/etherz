#include "test_framework.hpp"
#include "protocol/url.hpp"

TEST_CASE(url_parse_full) {
	auto url = etherz::protocol::Url::parse("http://example.com:8080/api/v1?key=val#section");
	CHECK_EQ(url.scheme, std::string("http"));
	CHECK_EQ(url.host, std::string("example.com"));
	CHECK_EQ(url.port, 8080);
	CHECK_EQ(url.path, std::string("/api/v1"));
	CHECK_EQ(url.query, std::string("key=val"));
	CHECK_EQ(url.fragment, std::string("section"));
}

TEST_CASE(url_parse_https_default_port) {
	auto url = etherz::protocol::Url::parse("https://secure.example.com/index.html");
	CHECK_EQ(url.scheme, std::string("https"));
	CHECK_EQ(url.host, std::string("secure.example.com"));
	CHECK_EQ(url.port, 443);
	CHECK_EQ(url.path, std::string("/index.html"));
}

TEST_CASE(url_parse_http_default_port) {
	auto url = etherz::protocol::Url::parse("http://localhost/");
	CHECK_EQ(url.scheme, std::string("http"));
	CHECK_EQ(url.host, std::string("localhost"));
	CHECK_EQ(url.port, 80);
}

TEST_CASE(url_to_string) {
	auto url = etherz::protocol::Url::parse("https://example.com/path");
	auto str = url.to_string();
	CHECK_TRUE(str.find("https://") == 0);
	CHECK_TRUE(str.find("example.com") != std::string::npos);
	CHECK_TRUE(str.find("/path") != std::string::npos);
}

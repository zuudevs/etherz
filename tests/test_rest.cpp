/**
 * @file test_rest.cpp
 * @brief Unit tests for REST client — URL building, auth, request construction
 */

#include "test_framework.hpp"
#include "protocol/rest_client.hpp"

namespace etp = etherz::protocol;

// ═══════════════════════════════════════════════
//  URL Building
// ═══════════════════════════════════════════════

TEST(rest_build_url_basic) {
	etp::RestClient api({.base_url = "http://api.example.com"});
	ASSERT_EQ(api.build_url("/users"), "http://api.example.com/users");
}

TEST(rest_build_url_trailing_slash) {
	etp::RestClient api({.base_url = "http://api.example.com/"});
	ASSERT_EQ(api.build_url("/users"), "http://api.example.com/users");
}

TEST(rest_build_url_no_base) {
	etp::RestClient api;
	ASSERT_EQ(api.build_url("http://example.com/users"),
		"http://example.com/users");
}

TEST(rest_build_url_with_query) {
	etp::RestClient api({.base_url = "http://api.example.com"});
	ASSERT_EQ(api.build_url("/search?q=test"),
		"http://api.example.com/search?q=test");
}

// ═══════════════════════════════════════════════
//  Bearer Auth
// ═══════════════════════════════════════════════

TEST(rest_auth_bearer) {
	auto auth = etp::RestAuth::bearer("my-token-123");
	ASSERT_EQ(auth.header_value(), "Bearer my-token-123");
	ASSERT_TRUE(auth.has_auth());
}

TEST(rest_auth_bearer_empty_token) {
	auto auth = etp::RestAuth::bearer("");
	ASSERT_EQ(auth.header_value(), "Bearer ");
	ASSERT_TRUE(auth.has_auth());
}

// ═══════════════════════════════════════════════
//  Basic Auth
// ═══════════════════════════════════════════════

TEST(rest_auth_basic) {
	auto auth = etp::RestAuth::basic("admin", "secret");
	auto val = auth.header_value();
	ASSERT_TRUE(val.find("Basic ") == 0);
	ASSERT_TRUE(auth.has_auth());
	// "admin:secret" base64 = "YWRtaW46c2VjcmV0"
	ASSERT_EQ(val, "Basic YWRtaW46c2VjcmV0");
}

TEST(rest_auth_none) {
	etp::RestAuth auth;
	ASSERT_TRUE(!auth.has_auth());
	ASSERT_TRUE(auth.header_value().empty());
}

// ═══════════════════════════════════════════════
//  RestResponse
// ═══════════════════════════════════════════════

TEST(rest_response_is_success) {
	etp::RestResponse res;
	res.status = etp::HttpStatus::Ok;
	ASSERT_TRUE(res.is_success());

	res.status = etp::HttpStatus::Created;
	ASSERT_TRUE(res.is_success());

	res.status = etp::HttpStatus::NotFound;
	ASSERT_TRUE(!res.is_success());
}

TEST(rest_response_status_code) {
	etp::RestResponse res;
	res.status = etp::HttpStatus::OK;
	ASSERT_EQ(res.status_code(), 200);
}

TEST(rest_response_is_json) {
	etp::RestResponse res;
	res.headers.set("Content-Type", "application/json; charset=utf-8");
	ASSERT_TRUE(res.is_json());

	res.headers.set("Content-Type", "text/html");
	ASSERT_TRUE(!res.is_json());
}

TEST(rest_response_parse_json) {
	etp::RestResponse res;
	res.body = R"({"name":"John","age":30})";

	json::Arena arena;
	auto result = res.parse_json(arena);
	ASSERT_TRUE(result.has_value());

	auto root = *result;
	ASSERT_TRUE(root->is_object());

	auto name = (*root)["name"];
	ASSERT_TRUE(name.has_value());
	ASSERT_TRUE((*name)->is_string());
	auto name_str = (*name)->as_string();
	ASSERT_TRUE(name_str.has_value());
	ASSERT_EQ(*name_str, "John");

	auto age = (*root)["age"];
	ASSERT_TRUE(age.has_value());
	ASSERT_TRUE((*age)->is_number());
	auto age_val = (*age)->as_number();
	ASSERT_TRUE(age_val.has_value());
	ASSERT_EQ(*age_val, 30.0);
}

TEST(rest_response_parse_json_array) {
	etp::RestResponse res;
	res.body = R"([1, 2, 3])";

	json::Arena arena;
	auto result = res.parse_json(arena);
	ASSERT_TRUE(result.has_value());

	auto root = *result;
	ASSERT_TRUE(root->is_array());
	ASSERT_EQ(root->size(), 3u);
}

// ═══════════════════════════════════════════════
//  JSON Body Building with zuu-json
// ═══════════════════════════════════════════════

TEST(rest_json_body_building) {
	json::Arena arena;
	auto body = json::build_object(arena)
		.add("name", "Alice")
		.add("email", "alice@example.com")
		.add("active", true)
		.add("score", 95.5)
		.build();

	auto json_str = json::write(body);
	ASSERT_TRUE(json_str.find("\"name\"") != std::string::npos);
	ASSERT_TRUE(json_str.find("\"Alice\"") != std::string::npos);
	ASSERT_TRUE(json_str.find("\"email\"") != std::string::npos);
	ASSERT_TRUE(json_str.find("true") != std::string::npos);
}

TEST(rest_json_nested_body) {
	json::Arena arena;
	auto address = json::build_object(arena)
		.add("city", "Tokyo")
		.add("zip", "100-0001")
		.build();

	auto body = json::build_object(arena)
		.add("name", "Bob")
		.add("address", address)
		.build();

	auto json_str = json::write(body);
	ASSERT_TRUE(json_str.find("\"address\"") != std::string::npos);
	ASSERT_TRUE(json_str.find("\"Tokyo\"") != std::string::npos);
}

// ═══════════════════════════════════════════════
//  RestClientConfig
// ═══════════════════════════════════════════════

TEST(rest_client_config_defaults) {
	etp::RestClientConfig cfg;
	ASSERT_TRUE(cfg.base_url.empty());
	ASSERT_TRUE(!cfg.auth.has_auth());
	ASSERT_EQ(cfg.user_agent, "Etherz/4.0.0");
}

TEST(rest_client_set_auth) {
	etp::RestClient api;
	api.set_auth(etp::RestAuth::bearer("token"));
	ASSERT_TRUE(api.auth().has_auth());
	ASSERT_EQ(api.auth().header_value(), "Bearer token");
}

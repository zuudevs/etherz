#include "test_framework.hpp"
#include "protocol/http.hpp"

namespace etp = etherz::protocol;

TEST_CASE(http_method_to_string) {
	CHECK_EQ(etp::method_string(etp::HttpMethod::Get), std::string_view("GET"));
	CHECK_EQ(etp::method_string(etp::HttpMethod::Post), std::string_view("POST"));
	CHECK_EQ(etp::method_string(etp::HttpMethod::Delete), std::string_view("DELETE"));
}

TEST_CASE(http_status_to_string) {
	CHECK_EQ(etp::status_text(etp::HttpStatus::OK), std::string_view("OK"));
	CHECK_EQ(etp::status_text(etp::HttpStatus::NotFound), std::string_view("Not Found"));
}

TEST_CASE(http_headers_case_insensitive) {
	etp::HttpHeaders h;
	h.set("Content-Type", "text/html");
	CHECK_TRUE(h.has("content-type"));
	CHECK_TRUE(h.has("CONTENT-TYPE"));
	CHECK_EQ(h.get("content-type"), std::string("text/html"));
}

TEST_CASE(http_request_serialize) {
	etp::HttpRequest req;
	req.method = etp::HttpMethod::Get;
	req.path = "/index.html";
	req.headers.set("Host", "example.com");
	auto raw = req.serialize();
	CHECK_TRUE(raw.find("GET /index.html HTTP/1.1") != std::string::npos);
	CHECK_TRUE(raw.find("Host: example.com") != std::string::npos);
}

TEST_CASE(http_response_parse) {
	std::string raw = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<h1>Hi</h1>";
	auto resp = etp::http_parser::parse_response(raw);
	CHECK_EQ(static_cast<uint16_t>(resp.status), static_cast<uint16_t>(200));
	CHECK_EQ(resp.body, std::string("<h1>Hi</h1>"));
	CHECK_EQ(resp.headers.get("Content-Type"), std::string("text/html"));
}

TEST_CASE(http_request_parse) {
	std::string raw = "POST /api HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ntest";
	auto req = etp::http_parser::parse_request(raw);
	CHECK_EQ(req.method, etp::HttpMethod::Post);
	CHECK_EQ(req.path, std::string("/api"));
	CHECK_EQ(req.body, std::string("test"));
}

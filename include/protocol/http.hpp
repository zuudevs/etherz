/**
 * @file http.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief HTTP/1.1 request, response, and parser
 * @version 1.0.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <print>

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  HTTP Method
// ═══════════════════════════════════════════════

enum class HttpMethod : uint8_t {
	Get, Post, Put, Delete, Patch, Head, Options, Unknown
};

inline constexpr std::string_view method_string(HttpMethod m) noexcept {
	switch (m) {
		case HttpMethod::Get:     return "GET";
		case HttpMethod::Post:    return "POST";
		case HttpMethod::Put:     return "PUT";
		case HttpMethod::Delete:  return "DELETE";
		case HttpMethod::Patch:   return "PATCH";
		case HttpMethod::Head:    return "HEAD";
		case HttpMethod::Options: return "OPTIONS";
		default:                  return "UNKNOWN";
	}
}

inline HttpMethod method_from_string(std::string_view s) noexcept {
	if (s == "GET")     return HttpMethod::Get;
	if (s == "POST")    return HttpMethod::Post;
	if (s == "PUT")     return HttpMethod::Put;
	if (s == "DELETE")  return HttpMethod::Delete;
	if (s == "PATCH")   return HttpMethod::Patch;
	if (s == "HEAD")    return HttpMethod::Head;
	if (s == "OPTIONS") return HttpMethod::Options;
	return HttpMethod::Unknown;
}

// ═══════════════════════════════════════════════
//  HTTP Status
// ═══════════════════════════════════════════════

enum class HttpStatus : uint16_t {
	OK                  = 200,
	Created             = 201,
	NoContent           = 204,
	MovedPermanently    = 301,
	Found               = 302,
	NotModified         = 304,
	BadRequest          = 400,
	Unauthorized        = 401,
	Forbidden           = 403,
	NotFound            = 404,
	MethodNotAllowed    = 405,
	InternalServerError = 500,
	NotImplemented      = 501,
	BadGateway          = 502,
	ServiceUnavailable  = 503,
	Unknown             = 0
};

inline constexpr std::string_view status_text(HttpStatus s) noexcept {
	switch (s) {
		case HttpStatus::OK:                  return "OK";
		case HttpStatus::Created:             return "Created";
		case HttpStatus::NoContent:           return "No Content";
		case HttpStatus::MovedPermanently:    return "Moved Permanently";
		case HttpStatus::Found:               return "Found";
		case HttpStatus::NotModified:         return "Not Modified";
		case HttpStatus::BadRequest:          return "Bad Request";
		case HttpStatus::Unauthorized:        return "Unauthorized";
		case HttpStatus::Forbidden:           return "Forbidden";
		case HttpStatus::NotFound:            return "Not Found";
		case HttpStatus::MethodNotAllowed:    return "Method Not Allowed";
		case HttpStatus::InternalServerError: return "Internal Server Error";
		case HttpStatus::NotImplemented:      return "Not Implemented";
		case HttpStatus::BadGateway:          return "Bad Gateway";
		case HttpStatus::ServiceUnavailable:  return "Service Unavailable";
		default:                              return "Unknown";
	}
}

// ═══════════════════════════════════════════════
//  HTTP Headers
// ═══════════════════════════════════════════════

class HttpHeaders {
public:
	using Entry = std::pair<std::string, std::string>;

	void set(std::string key, std::string value) {
		for (auto& [k, v] : entries_) {
			if (iequals(k, key)) { v = std::move(value); return; }
		}
		entries_.emplace_back(std::move(key), std::move(value));
	}

	std::string_view get(std::string_view key) const noexcept {
		for (const auto& [k, v] : entries_) {
			if (iequals(k, key)) return v;
		}
		return {};
	}

	bool has(std::string_view key) const noexcept {
		for (const auto& [k, v] : entries_) {
			if (iequals(k, key)) return true;
		}
		return false;
	}

	size_t size() const noexcept { return entries_.size(); }
	const std::vector<Entry>& entries() const noexcept { return entries_; }

	/**
	 * @brief Serialize headers to HTTP format
	 */
	std::string serialize() const {
		std::string s;
		for (const auto& [k, v] : entries_) {
			s += k + ": " + v + "\r\n";
		}
		return s;
	}

private:
	std::vector<Entry> entries_;

	static bool iequals(std::string_view a, std::string_view b) noexcept {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i) {
			char ca = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] + 32) : a[i];
			char cb = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] + 32) : b[i];
			if (ca != cb) return false;
		}
		return true;
	}
};

// ═══════════════════════════════════════════════
//  HTTP Request
// ═══════════════════════════════════════════════

struct HttpRequest {
	HttpMethod  method = HttpMethod::Get;
	std::string path   = "/";
	std::string version = "HTTP/1.1";
	HttpHeaders headers;
	std::string body;

	/**
	 * @brief Serialize to raw HTTP request bytes
	 */
	std::string serialize() const {
		std::string s;
		s += std::string(method_string(method)) + " " + path + " " + version + "\r\n";
		s += headers.serialize();
		if (!body.empty() && !headers.has("Content-Length")) {
			s += "Content-Length: " + std::to_string(body.size()) + "\r\n";
		}
		s += "\r\n";
		s += body;
		return s;
	}

	inline void display() const noexcept {
		std::print("HTTP Request: {} {} {}\n", method_string(method), path, version);
	}
};

// ═══════════════════════════════════════════════
//  HTTP Response
// ═══════════════════════════════════════════════

struct HttpResponse {
	std::string version = "HTTP/1.1";
	HttpStatus  status  = HttpStatus::OK;
	HttpHeaders headers;
	std::string body;

	/**
	 * @brief Serialize to raw HTTP response bytes
	 */
	std::string serialize() const {
		std::string s;
		s += version + " " + std::to_string(static_cast<uint16_t>(status))
		   + " " + std::string(status_text(status)) + "\r\n";
		s += headers.serialize();
		if (!body.empty() && !headers.has("Content-Length")) {
			s += "Content-Length: " + std::to_string(body.size()) + "\r\n";
		}
		s += "\r\n";
		s += body;
		return s;
	}

	inline void display() const noexcept {
		std::print("HTTP Response: {} {} {}\n", version,
			static_cast<uint16_t>(status), status_text(status));
	}
};

// ═══════════════════════════════════════════════
//  HTTP Parser
// ═══════════════════════════════════════════════

namespace http_parser {

namespace detail {
	inline size_t find_crlf(std::string_view s, size_t start = 0) noexcept {
		for (size_t i = start; i + 1 < s.size(); ++i) {
			if (s[i] == '\r' && s[i + 1] == '\n') return i;
		}
		return std::string_view::npos;
	}

	inline void parse_headers(std::string_view s, HttpHeaders& headers) {
		size_t pos = 0;
		while (pos < s.size()) {
			auto line_end = find_crlf(s, pos);
			if (line_end == std::string_view::npos || line_end == pos) break;
			auto line = s.substr(pos, line_end - pos);
			auto colon = line.find(':');
			if (colon != std::string_view::npos) {
				auto key = line.substr(0, colon);
				auto val = line.substr(colon + 1);
				// Trim leading whitespace
				while (!val.empty() && val[0] == ' ') val.remove_prefix(1);
				headers.set(std::string(key), std::string(val));
			}
			pos = line_end + 2;
		}
	}
} // namespace detail

/**
 * @brief Parse raw HTTP request string
 */
inline HttpRequest parse_request(std::string_view raw) {
	HttpRequest req;

	auto first_line_end = detail::find_crlf(raw);
	if (first_line_end == std::string_view::npos) return req;

	// Parse request line: METHOD PATH VERSION
	auto line = raw.substr(0, first_line_end);
	auto sp1 = line.find(' ');
	if (sp1 == std::string_view::npos) return req;
	auto sp2 = line.find(' ', sp1 + 1);

	req.method = method_from_string(line.substr(0, sp1));
	if (sp2 != std::string_view::npos) {
		req.path = std::string(line.substr(sp1 + 1, sp2 - sp1 - 1));
		req.version = std::string(line.substr(sp2 + 1));
	} else {
		req.path = std::string(line.substr(sp1 + 1));
	}

	// Find header/body boundary
	size_t header_start = first_line_end + 2;
	std::string_view double_crlf = "\r\n\r\n";
	auto body_start = raw.find(double_crlf, header_start);
	if (body_start != std::string_view::npos) {
		detail::parse_headers(raw.substr(header_start, body_start - header_start + 2), req.headers);
		req.body = std::string(raw.substr(body_start + 4));
	} else {
		detail::parse_headers(raw.substr(header_start), req.headers);
	}

	return req;
}

/**
 * @brief Parse raw HTTP response string
 */
inline HttpResponse parse_response(std::string_view raw) {
	HttpResponse resp;

	auto first_line_end = detail::find_crlf(raw);
	if (first_line_end == std::string_view::npos) return resp;

	// Parse status line: VERSION STATUS TEXT
	auto line = raw.substr(0, first_line_end);
	auto sp1 = line.find(' ');
	if (sp1 == std::string_view::npos) return resp;
	auto sp2 = line.find(' ', sp1 + 1);

	resp.version = std::string(line.substr(0, sp1));
	auto status_str = (sp2 != std::string_view::npos)
		? line.substr(sp1 + 1, sp2 - sp1 - 1) : line.substr(sp1 + 1);

	uint32_t code = 0;
	for (char c : status_str) {
		if (c >= '0' && c <= '9') code = code * 10 + static_cast<uint32_t>(c - '0');
	}
	resp.status = static_cast<HttpStatus>(code);

	// Headers and body
	size_t header_start = first_line_end + 2;
	std::string_view double_crlf = "\r\n\r\n";
	auto body_start = raw.find(double_crlf, header_start);
	if (body_start != std::string_view::npos) {
		detail::parse_headers(raw.substr(header_start, body_start - header_start + 2), resp.headers);
		resp.body = std::string(raw.substr(body_start + 4));
	} else {
		detail::parse_headers(raw.substr(header_start), resp.headers);
	}

	return resp;
}

} // namespace http_parser

} // namespace protocol
} // namespace etherz

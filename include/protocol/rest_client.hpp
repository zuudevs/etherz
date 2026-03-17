/**
 * @file rest_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief REST client layered on HttpClient with zuu-json integration
 * @version 3.3.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <expected>
#include <functional>
#include <vector>
#include <utility>

#include "http_client.hpp"
#include "http.hpp"
#include "url.hpp"
#include "../core/error.hpp"

// zuu-json from third_party
#include "cpp_json.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  REST Response
// ═══════════════════════════════════════════════

/**
 * @brief A REST API response with JSON parsing support
 */
struct RestResponse {
	HttpStatus   status;
	HttpHeaders  headers;
	std::string  body;

	/**
	 * @brief Parse the response body as JSON
	 * @param arena Arena for JSON node allocation
	 * @return Parsed JSON root node, or error
	 */
	json::Result<json::Node*> parse_json(json::Arena& arena) const {
		return json::parse(body, arena);
	}

	/**
	 * @brief Get the HTTP status code as integer
	 */
	int status_code() const noexcept {
		return static_cast<int>(status);
	}

	/**
	 * @brief Check if the response indicates success (2xx)
	 */
	bool is_success() const noexcept {
		int code = status_code();
		return code >= 200 && code < 300;
	}

	/**
	 * @brief Check if the response is JSON
	 */
	bool is_json() const noexcept {
		auto ct = headers.get("Content-Type");
		return ct.find("application/json") != std::string::npos
			|| ct.find("text/json") != std::string::npos;
	}
};

// ═══════════════════════════════════════════════
//  REST Authentication
// ═══════════════════════════════════════════════

/**
 * @brief Authentication configuration for REST requests
 */
struct RestAuth {
	enum class Type : uint8_t {
		None,
		Bearer,
		Basic
	};

	Type        type = Type::None;
	std::string token;       // Bearer token or Base64 credentials
	std::string username;    // For Basic auth
	std::string password;    // For Basic auth

	/**
	 * @brief Create Bearer token auth
	 */
	static RestAuth bearer(std::string token) {
		RestAuth auth;
		auth.type = Type::Bearer;
		auth.token = std::move(token);
		return auth;
	}

	/**
	 * @brief Create Basic auth
	 */
	static RestAuth basic(std::string user, std::string pass) {
		RestAuth auth;
		auth.type = Type::Basic;
		auth.username = std::move(user);
		auth.password = std::move(pass);
		return auth;
	}

	/**
	 * @brief Generate the Authorization header value
	 */
	std::string header_value() const {
		switch (type) {
			case Type::Bearer:
				return "Bearer " + token;
			case Type::Basic:
				return "Basic " + base64_encode(username + ":" + password);
			default:
				return "";
		}
	}

	bool has_auth() const noexcept { return type != Type::None; }

private:
	static std::string base64_encode(std::string_view input) {
		static constexpr std::string_view chars =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string result;
		result.reserve(((input.size() + 2) / 3) * 4);

		size_t i = 0;
		while (i < input.size()) {
			uint32_t a = static_cast<uint8_t>(input[i++]);
			uint32_t b = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t c = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t triple = (a << 16) | (b << 8) | c;

			result += chars[(triple >> 18) & 0x3F];
			result += chars[(triple >> 12) & 0x3F];
			result += (i > input.size() + 1) ? '=' : chars[(triple >> 6) & 0x3F];
			result += (i > input.size()) ? '=' : chars[triple & 0x3F];
		}

		return result;
	}
};

// ═══════════════════════════════════════════════
//  REST Client Configuration
// ═══════════════════════════════════════════════

struct RestClientConfig {
	std::string  base_url;         // Base URL prefix (e.g. "http://api.example.com")
	RestAuth     auth;             // Default auth for all requests
	std::string  user_agent = "Etherz/4.0.0";
	std::vector<std::pair<std::string, std::string>> default_headers;
};

// ═══════════════════════════════════════════════
//  REST Client
// ═══════════════════════════════════════════════

/**
 * @brief REST client with JSON support via zuu-json
 * 
 * Provides typed REST verbs (GET, POST, PUT, PATCH, DELETE) layered
 * on HttpClient, with automatic JSON content-type handling and
 * authentication helpers.
 * 
 * Usage:
 *   RestClient api({"http://api.example.com"});
 *   api.set_auth(RestAuth::bearer("my-token"));
 *   
 *   // GET /users
 *   auto res = api.get("/users");
 *   json::Arena arena;
 *   auto root = res->parse_json(arena);
 *   
 *   // POST /users with JSON body
 *   json::Arena body_arena;
 *   auto body = json::build_object(body_arena)
 *       .add("name", "John")
 *       .add("email", "john@example.com")
 *       .build();
 *   api.post_json("/users", body);
 */
class RestClient {
public:
	explicit RestClient(RestClientConfig config = {}) noexcept
		: config_(std::move(config)) {}

	// ─── Configuration ──────────────────

	void set_base_url(std::string url) { config_.base_url = std::move(url); }
	void set_auth(RestAuth auth) { config_.auth = std::move(auth); }

	void add_default_header(std::string key, std::string value) {
		config_.default_headers.emplace_back(std::move(key), std::move(value));
	}

	// ─── REST Verbs ─────────────────────

	/**
	 * @brief Perform a GET request
	 */
	std::expected<RestResponse, core::Error> get(std::string_view path) {
		return request(HttpMethod::Get, path, "");
	}

	/**
	 * @brief Perform a POST request with string body
	 */
	std::expected<RestResponse, core::Error> post(
		std::string_view path, std::string body,
		std::string_view content_type = "application/json")
	{
		return request(HttpMethod::Post, path, std::move(body), content_type);
	}

	/**
	 * @brief Perform a POST request with JSON body
	 */
	std::expected<RestResponse, core::Error> post_json(
		std::string_view path, const json::Node* body)
	{
		std::string json_body = json::write(body);
		return post(path, std::move(json_body));
	}

	/**
	 * @brief Perform a PUT request with string body
	 */
	std::expected<RestResponse, core::Error> put(
		std::string_view path, std::string body,
		std::string_view content_type = "application/json")
	{
		return request(HttpMethod::Put, path, std::move(body), content_type);
	}

	/**
	 * @brief Perform a PUT request with JSON body
	 */
	std::expected<RestResponse, core::Error> put_json(
		std::string_view path, const json::Node* body)
	{
		std::string json_body = json::write(body);
		return put(path, std::move(json_body));
	}

	/**
	 * @brief Perform a PATCH request with string body
	 */
	std::expected<RestResponse, core::Error> patch(
		std::string_view path, std::string body,
		std::string_view content_type = "application/json")
	{
		return request(HttpMethod::Patch, path, std::move(body), content_type);
	}

	/**
	 * @brief Perform a PATCH request with JSON body
	 */
	std::expected<RestResponse, core::Error> patch_json(
		std::string_view path, const json::Node* body)
	{
		std::string json_body = json::write(body);
		return patch(path, std::move(json_body));
	}

	/**
	 * @brief Perform a DELETE request
	 */
	std::expected<RestResponse, core::Error> del(std::string_view path) {
		return request(HttpMethod::Delete, path, "");
	}

	// ─── JSON Helpers (public for testing) ───

	/**
	 * @brief Build a full URL from base + path
	 */
	std::string build_url(std::string_view path) const {
		std::string url = config_.base_url;
		if (!url.empty() && url.back() == '/' && !path.empty() && path[0] == '/') {
			url.pop_back();
		}
		url += path;
		return url;
	}

	/**
	 * @brief Get current auth configuration
	 */
	const RestAuth& auth() const noexcept { return config_.auth; }

private:
	RestClientConfig config_;
	HttpClient       http_;

	/**
	 * @brief Perform an HTTP request and return a RestResponse
	 */
	std::expected<RestResponse, core::Error> request(
		HttpMethod method, std::string_view path,
		std::string body,
		std::string_view content_type = "application/json")
	{
		std::string full_url = build_url(path);
		auto url = Url::parse(full_url);
		if (url.host.empty()) {
			return std::unexpected(core::Error::InvalidAddress);
		}

		HttpRequest req;
		req.method = method;
		req.path = url.path.empty() ? "/" : url.path;
		if (!url.query.empty()) req.path += "?" + url.query;

		// Standard headers
		req.headers.set("Host", url.host);
		req.headers.set("Connection", "close");
		req.headers.set("User-Agent", config_.user_agent);
		req.headers.set("Accept", "application/json");

		// Default headers
		for (const auto& [key, val] : config_.default_headers) {
			req.headers.set(key, val);
		}

		// Auth
		if (config_.auth.has_auth()) {
			req.headers.set("Authorization", config_.auth.header_value());
		}

		// Body
		if (!body.empty()) {
			req.headers.set("Content-Type", std::string(content_type));
			req.headers.set("Content-Length", std::to_string(body.size()));
			req.body = std::move(body);
		}

		auto result = http_.send_request(url, req);
		if (!result.has_value()) {
			return std::unexpected(result.error());
		}

		RestResponse response;
		response.status  = result->status;
		response.headers = std::move(result->headers);
		response.body    = std::move(result->body);
		return response;
	}
};

} // namespace protocol
} // namespace etherz

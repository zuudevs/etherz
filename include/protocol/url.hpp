/**
 * @file url.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief URL parsing utility
 * @version 0.4.0
 * @date 2026-02-19
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <print>

namespace etherz {
namespace protocol {

/**
 * @brief Parsed URL components
 * 
 * Supports: scheme://host:port/path?query#fragment
 */
struct Url {
	std::string scheme;
	std::string host;
	uint16_t    port = 0;
	std::string path;
	std::string query;
	std::string fragment;

	/**
	 * @brief Parse a URL string into components
	 */
	static Url parse(std::string_view url) noexcept {
		Url result;
		size_t pos = 0;

		// Scheme
		auto scheme_end = url.find("://");
		if (scheme_end != std::string_view::npos) {
			result.scheme = std::string(url.substr(0, scheme_end));
			pos = scheme_end + 3;
		}

		// Fragment (split early)
		auto frag_pos = url.find('#', pos);
		std::string_view before_frag = (frag_pos != std::string_view::npos)
			? url.substr(pos, frag_pos - pos) : url.substr(pos);
		if (frag_pos != std::string_view::npos) {
			result.fragment = std::string(url.substr(frag_pos + 1));
		}

		// Query (split from before_frag)
		auto query_pos = before_frag.find('?');
		std::string_view before_query = (query_pos != std::string_view::npos)
			? before_frag.substr(0, query_pos) : before_frag;
		if (query_pos != std::string_view::npos) {
			result.query = std::string(before_frag.substr(query_pos + 1));
		}

		// Host:port / path
		auto path_pos = before_query.find('/');
		std::string_view authority = (path_pos != std::string_view::npos)
			? before_query.substr(0, path_pos) : before_query;
		if (path_pos != std::string_view::npos) {
			result.path = std::string(before_query.substr(path_pos));
		} else {
			result.path = "/";
		}

		// Split host:port
		auto colon = authority.rfind(':');
		if (colon != std::string_view::npos) {
			result.host = std::string(authority.substr(0, colon));
			auto port_str = authority.substr(colon + 1);
			uint32_t p = 0;
			for (char c : port_str) {
				if (c < '0' || c > '9') { p = 0; break; }
				p = p * 10 + static_cast<uint32_t>(c - '0');
			}
			result.port = (p <= 65535) ? static_cast<uint16_t>(p) : 0;
		} else {
			result.host = std::string(authority);
		}

		// Default ports
		if (result.port == 0) {
			if (result.scheme == "http")  result.port = 80;
			if (result.scheme == "https") result.port = 443;
			if (result.scheme == "ws")    result.port = 80;
			if (result.scheme == "wss")   result.port = 443;
		}

		return result;
	}

	/**
	 * @brief Reconstruct URL string
	 */
	std::string to_string() const {
		std::string s;
		if (!scheme.empty()) s += scheme + "://";
		s += host;
		if (port != 0 && port != 80 && port != 443) {
			s += ":" + std::to_string(port);
		}
		s += path;
		if (!query.empty()) s += "?" + query;
		if (!fragment.empty()) s += "#" + fragment;
		return s;
	}

	inline void display() const noexcept {
		std::print("URL: scheme={}, host={}, port={}, path={}, query={}, fragment={}\n",
			scheme, host, port, path, query, fragment);
	}
};

} // namespace protocol
} // namespace etherz

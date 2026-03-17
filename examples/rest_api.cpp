/**
 * @file rest_api.cpp
 * @brief REST API client example with zuu-json
 * 
 * Demonstrates:
 * - RestClient with base URL and bearer auth
 * - Building JSON request bodies with zuu-json ObjectBuilder
 * - Parsing JSON responses
 */

#include "protocol/rest_client.hpp"
#include <print>

namespace etp = etherz::protocol;

int main() {
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz REST Client Example\n");
	std::print("═══════════════════════════════════\n\n");

	// ── Create REST Client ──────────────
	etp::RestClient api({
		.base_url   = "http://jsonplaceholder.typicode.com",
		.user_agent = "Etherz/4.0.0"
	});

	// ── GET /posts/1 ────────────────────
	std::print("── GET /posts/1 ────────────────────\n");
	auto get_res = api.get("/posts/1");
	if (get_res.has_value()) {
		std::print("  Status: {}\n", get_res->status_code());
		std::print("  JSON?:  {}\n", get_res->is_json());

		json::Arena arena;
		auto parsed = get_res->parse_json(arena);
		if (parsed.has_value()) {
			auto root = *parsed;
			if (root->is_object()) {
				auto title = (*root)["title"];
				if (title.has_value() && (*title)->is_string()) {
					auto sv = (*title)->as_string();
					if (sv.has_value()) {
						std::print("  Title:  {}\n", *sv);
					}
				}
			}
		}
	} else {
		std::print("  Error: {}\n",
			etherz::core::error_message(get_res.error()));
	}

	// ── POST /posts ─────────────────────
	std::print("\n── POST /posts ─────────────────────\n");

	json::Arena body_arena;
	auto body = json::build_object(body_arena)
		.add("title", "etherz test post")
		.add("body", "This was sent by the Etherz REST client")
		.add("userId", 1)
		.build();

	auto post_res = api.post_json("/posts", body);
	if (post_res.has_value()) {
		std::print("  Status: {}\n", post_res->status_code());
		std::print("  Body:   {}\n",
			post_res->body.substr(0, 120));
	} else {
		std::print("  Error: {}\n",
			etherz::core::error_message(post_res.error()));
	}

	// ── Auth Example ────────────────────
	std::print("\n── Bearer Auth Example ─────────────\n");
	api.set_auth(etp::RestAuth::bearer("my-api-token-123"));
	std::print("  Auth header: {}\n", api.auth().header_value());

	etp::RestAuth basic = etp::RestAuth::basic("user", "pass");
	std::print("  Basic auth:  {}\n", basic.header_value());

	std::print("\nDone.\n");
	return 0;
}

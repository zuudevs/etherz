/**
 * @file service_browser.cpp
 * @brief mDNS/DNS-SD service discovery example
 */

#include "../include/net/service_discovery.hpp"
#include "../include/net/mdns.hpp"
#include <print>
#include <string>

namespace en = etherz::net;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: service_browser <browse|register>\n");
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "browse") {
		std::print("Browsing for HTTP services...\n\n");

		en::ServiceBrowser browser;
		auto err = browser.browse("_http._tcp.local",
			[](const en::ServiceInfo& svc) {
				std::print("Found service:\n");
				std::print("  Name: {}\n", svc.name);
				std::print("  Type: {}\n", svc.type);
				std::print("  Host: {}\n", svc.hostname);
				std::print("  Port: {}\n", svc.port);
				for (const auto& [key, value] : svc.txt) {
					std::print("  TXT: {}={}\n", key, value);
				}
				std::print("\n");
			});

		if (ec::is_error(err)) {
			std::print("Browse failed: {}\n", ec::error_message(err));
		}
	}
	else if (mode == "register") {
		std::print("Registering HTTP service...\n");

		en::ServiceRegistrar registrar;
		registrar.register_service({
			.name = "Etherz Web Server",
			.type = "_http._tcp.local",
			.hostname = "etherz-host.local",
			.address = en::Ip<4>(192, 168, 1, 100),
			.port = 8080,
			.txt = {{"path", "/api"}, {"version", "2.5.0"}}
		});

		registrar.announce();
		std::print("Service registered\n");

		// Respond to queries
		en::MdnsResponder responder;
		responder.register_host("etherz-host.local",
			en::Ip<4>(192, 168, 1, 100));
		std::print("mDNS responder running (Ctrl+C to stop)\n");
		responder.serve();
	}

	return 0;
}

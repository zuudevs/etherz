/**
 * @file dns_lookup.cpp
 * @brief DNS lookup tool example
 * 
 * Demonstrates Dns::resolve and Dns::reverse.
 * Usage: dns_lookup <hostname>
 */

#include "etherz.hpp"
#include "net/dns.hpp"
#include <print>

#ifdef _WIN32
	#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	if (argc < 2) {
		std::print("Usage: dns_lookup <hostname>\n");
		std::print("Example: dns_lookup google.com\n");
		return 1;
	}

	std::string hostname = argv[1];

	std::print("╔═══════════════════════════════╗\n");
	std::print("║  Etherz DNS Lookup v1.0.0     ║\n");
	std::print("╚═══════════════════════════════╝\n\n");

	std::print("Resolving: {}\n\n", hostname);

	auto result = etherz::net::Dns::resolve(hostname);
	if (!result.success) {
		std::print("Failed to resolve '{}'\n", hostname);
		return 1;
	}

	if (!result.canonical_name.empty()) {
		std::print("Canonical: {}\n", result.canonical_name);
	}

	std::print("Found {} address(es)\n\n", result.count());

	for (const auto& ip : result.ipv4_addresses) {
		auto b = ip.bytes();
		std::print("  IPv4: {}.{}.{}.{}\n", b[0], b[1], b[2], b[3]);
	}
	for (const auto& ip : result.ipv6_addresses) {
		std::print("  IPv6: ");
		ip.display();
	}

	// Reverse lookup for first IPv4
	if (!result.ipv4_addresses.empty()) {
		auto rev = etherz::net::Dns::reverse(result.ipv4_addresses[0]);
		std::print("\nReverse: {}\n",
			rev.empty() ? "(no PTR record)" : rev);
	}

	return 0;
}

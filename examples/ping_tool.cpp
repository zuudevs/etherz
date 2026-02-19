/**
 * @file ping_tool.cpp
 * @brief ICMP ping tool example
 * 
 * Demonstrates ping() utility.
 * Usage: ping_tool <ip>
 */

#include "etherz.hpp"
#include "net/internet_protocol.hpp"
#include "net/ping.hpp"
#include <print>

#ifdef _WIN32
	#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	std::string target = "127.0.0.1";
	if (argc > 1) target = argv[1];

	std::print("╔═══════════════════════════════╗\n");
	std::print("║  Etherz Ping Tool v1.0.0      ║\n");
	std::print("╚═══════════════════════════════╝\n\n");

	auto ip = etherz::net::Ip<4>(target);
	auto b = ip.bytes();
	std::print("Pinging {}.{}.{}.{} ...\n\n", b[0], b[1], b[2], b[3]);

	for (int i = 0; i < 4; ++i) {
		auto result = etherz::net::ping(ip, 2000);
		if (result.status == etherz::net::PingStatus::Success) {
			std::print("  Reply: rtt={}ms  ttl={}  bytes={}\n",
				result.rtt_ms, result.ttl, result.data_len);
		} else {
			std::print("  {}\n", etherz::net::ping_status_name(result.status));
		}
	}

	std::print("\nDone.\n");
	return 0;
}

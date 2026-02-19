/**
 * @file subnet_calc.cpp
 * @brief Subnet calculator example
 * 
 * Demonstrates Subnet<Ip<4>> CIDR utilities.
 * Usage: subnet_calc <cidr> [ip]
 */

#include "etherz.hpp"
#include "net/internet_protocol.hpp"
#include "net/subnet.hpp"
#include <print>

#ifdef _WIN32
	#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	if (argc < 2) {
		std::print("Usage: subnet_calc <cidr> [ip-to-check]\n");
		std::print("Example: subnet_calc 192.168.1.0/24 192.168.1.50\n");
		return 1;
	}

	std::print("╔═══════════════════════════════╗\n");
	std::print("║  Etherz Subnet Calc v1.0.0    ║\n");
	std::print("╚═══════════════════════════════╝\n\n");

	auto subnet = etherz::net::Subnet<etherz::net::Ip<4>>::parse(argv[1]);
	auto net = subnet.network().bytes();
	auto mask = subnet.mask().bytes();
	auto bcast = subnet.broadcast().bytes();

	std::print("CIDR      : {}\n", subnet.to_string());
	std::print("Network   : {}.{}.{}.{}\n", net[0], net[1], net[2], net[3]);
	std::print("Mask      : {}.{}.{}.{}\n", mask[0], mask[1], mask[2], mask[3]);
	std::print("Broadcast : {}.{}.{}.{}\n", bcast[0], bcast[1], bcast[2], bcast[3]);
	std::print("Prefix    : /{}\n", subnet.prefix_length());
	std::print("Hosts     : {}\n", subnet.host_count());

	if (argc > 2) {
		auto check_ip = etherz::net::Ip<4>(std::string_view(argv[2]));
		auto cb = check_ip.bytes();
		std::print("\nContains {}.{}.{}.{}? {}\n",
			cb[0], cb[1], cb[2], cb[3],
			subnet.contains(check_ip) ? "Yes" : "No");
	}

	return 0;
}

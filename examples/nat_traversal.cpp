/**
 * @file nat_traversal.cpp
 * @brief NAT traversal example — UPnP discovery + STUN query
 * 
 * Demonstrates:
 * - SSDP discovery of UPnP gateway devices
 * - Fetching control URL from device XML
 * - Adding/removing port mappings
 * - STUN binding request for public IP discovery
 */

#include "net/upnp.hpp"
#include "net/stun.hpp"
#include <print>

namespace etn = etherz::net;

int main() {
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz NAT Traversal Example\n");
	std::print("═══════════════════════════════════\n\n");

	// ── STUN: Discover Public IP ──────────
	std::print("── STUN Query ──────────────────────\n");
	std::print("Querying STUN server: stun.l.google.com:19302\n");

	auto stun = etn::StunClient::query("stun.l.google.com", 19302);
	if (stun.success) {
		std::print("  Public IP:   {}\n", stun.public_ip.display());
		std::print("  Public Port: {}\n", stun.public_port);
		std::print("  NAT Type:    {}\n", etn::nat_type_string(stun.nat_type));
	} else {
		std::print("  STUN query failed (no response)\n");
	}

	// ── UPnP: Discover Gateway ───────────
	std::print("\n── UPnP Discovery ──────────────────\n");
	std::print("Searching for UPnP IGD devices...\n");

	auto devices = etn::UpnpClient::discover(3000);
	std::print("  Found {} device(s)\n", devices.size());

	if (!devices.empty()) {
		auto& gw = devices[0];
		std::print("  Location: {}\n", gw.location);
		std::print("  Server:   {}\n", gw.server);

		// Fetch control URL
		auto err = etn::UpnpClient::fetch_control_url(gw);
		if (etherz::core::is_ok(err)) {
			std::print("  Control:  {}\n", gw.control_url);
			std::print("  Service:  {}\n", gw.service_type);

			// Get external IP via UPnP
			auto ext_ip = etn::UpnpClient::get_external_ip(gw);
			if (ext_ip.has_value()) {
				std::print("  External: {}\n", *ext_ip);
			}

			// Add a demo port mapping
			etn::PortMapping mapping;
			mapping.external_port   = 9999;
			mapping.internal_port   = 9999;
			mapping.internal_client = "192.168.1.100";
			mapping.protocol        = etn::PortProtocol::TCP;
			mapping.description     = "etherz-demo";
			mapping.lease_duration  = 300;  // 5 minutes

			auto map_err = etn::UpnpClient::add_port_mapping(gw, mapping);
			if (etherz::core::is_ok(map_err)) {
				std::print("  Mapped:   :{} -> {}:{} (TCP)\n",
					mapping.external_port, mapping.internal_client,
					mapping.internal_port);

				// Clean up
				etn::UpnpClient::delete_port_mapping(
					gw, 9999, etn::PortProtocol::TCP);
				std::print("  Unmapped: :9999 (TCP)\n");
			} else {
				std::print("  Port mapping failed: {}\n",
					etherz::core::error_message(map_err));
			}
		} else {
			std::print("  Failed to fetch control URL: {}\n",
				etherz::core::error_message(err));
		}
	}

	std::print("\nDone.\n");
	return 0;
}

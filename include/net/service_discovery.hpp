/**
 * @file service_discovery.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief DNS-SD service discovery (RFC 6763)
 * @version 2.5.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <unordered_map>

#include "mdns.hpp"
#include "multicast_socket.hpp"
#include "internet_protocol.hpp"
#include "socket_address.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Service Description
// ═══════════════════════════════════════════════

/**
 * @brief A network service discovered via DNS-SD
 */
struct ServiceInfo {
	std::string name;           // Human-readable service name
	std::string type;           // Service type (e.g., "_http._tcp.local")
	std::string hostname;       // Host providing the service
	Ip<4>       address;        // Resolved IP address
	uint16_t    port = 0;       // Service port
	std::unordered_map<std::string, std::string> txt;  // TXT record key-value pairs

	std::string full_name() const {
		return name + "." + type;
	}
};

// ═══════════════════════════════════════════════
//  Service Browser
// ═══════════════════════════════════════════════

/**
 * @brief Browse for services on the local network using DNS-SD
 * 
 * Usage:
 *   ServiceBrowser browser;
 *   browser.browse("_http._tcp.local",
 *       [](const ServiceInfo& svc) {
 *           std::print("Found: {} at {}:{}\n",
 *               svc.name, svc.address, svc.port);
 *       });
 */
class ServiceBrowser {
public:
	using DiscoverCallback = std::function<void(const ServiceInfo&)>;

	ServiceBrowser() noexcept = default;

	/**
	 * @brief Browse for services of a given type
	 * @param service_type DNS-SD service type (e.g., "_http._tcp.local")
	 * @param callback Called for each discovered service
	 */
	core::Error browse(std::string_view service_type, DiscoverCallback callback) {
		MdnsQuery query;
		return query.query(service_type, DnsRecordType::PTR,
			[&](const DnsRecord& record, const Ip<4>& sender) {
				ServiceInfo info;
				info.type = std::string(service_type);
				info.address = sender;

				// PTR rdata contains the service instance name
				if (record.type == DnsRecordType::PTR && !record.rdata.empty()) {
					size_t pos = 0;
					auto rdata_span = std::span<const uint8_t>(record.rdata);
					info.name = dns_decode_name(rdata_span, pos);
				}

				if (record.type == DnsRecordType::SRV && record.rdata.size() >= 6) {
					info.port = (static_cast<uint16_t>(record.rdata[4]) << 8)
					          | record.rdata[5];
					size_t pos = 6;
					auto rdata_span = std::span<const uint8_t>(record.rdata);
					info.hostname = dns_decode_name(rdata_span, pos);
				}

				if (record.type == DnsRecordType::A && record.rdata.size() >= 4) {
					info.address = Ip<4>(record.rdata[0], record.rdata[1],
						record.rdata[2], record.rdata[3]);
				}

				if (record.type == DnsRecordType::TXT) {
					parse_txt_record(record.rdata, info.txt);
				}

				callback(info);
			});
	}

	/**
	 * @brief Browse for all service types on the local network
	 */
	core::Error browse_all(DiscoverCallback callback) {
		return browse("_services._dns-sd._udp.local", callback);
	}

private:
	void parse_txt_record(const std::vector<uint8_t>& rdata,
		std::unordered_map<std::string, std::string>& txt)
	{
		size_t pos = 0;
		while (pos < rdata.size()) {
			uint8_t len = rdata[pos++];
			if (pos + len > rdata.size()) break;
			std::string entry(reinterpret_cast<const char*>(rdata.data() + pos), len);
			pos += len;

			auto eq = entry.find('=');
			if (eq != std::string::npos) {
				txt[entry.substr(0, eq)] = entry.substr(eq + 1);
			} else {
				txt[entry] = "";
			}
		}
	}
};

// ═══════════════════════════════════════════════
//  Service Registrar
// ═══════════════════════════════════════════════

/**
 * @brief Register services for DNS-SD discovery
 * 
 * Usage:
 *   ServiceRegistrar reg;
 *   reg.register_service({
 *       .name = "My Web Server",
 *       .type = "_http._tcp.local",
 *       .port = 8080
 *   });
 *   reg.announce();
 */
class ServiceRegistrar {
public:
	ServiceRegistrar() noexcept = default;

	/**
	 * @brief Register a service
	 */
	void register_service(ServiceInfo service) {
		services_.push_back(std::move(service));
	}

	/**
	 * @brief Remove a registered service
	 */
	void unregister_service(std::string_view name) {
		services_.erase(
			std::remove_if(services_.begin(), services_.end(),
				[&](const ServiceInfo& s) { return s.name == name; }),
			services_.end());
	}

	/**
	 * @brief Send mDNS announcements for all registered services
	 */
	core::Error announce() {
		MdnsResponder responder;
		for (const auto& svc : services_) {
			responder.register_host(svc.full_name(), svc.address);

			// Register TXT records
			for (const auto& [key, value] : svc.txt) {
				responder.register_txt(svc.full_name(), key + "=" + value);
			}
		}

		// Could block — for one-shot announcement, use a separate thread
		// responder.serve();
		return core::Error::None;
	}

	const std::vector<ServiceInfo>& services() const noexcept { return services_; }

private:
	std::vector<ServiceInfo> services_;
};

} // namespace net
} // namespace etherz

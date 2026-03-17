/**
 * @file upnp.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief UPnP IGD client — SSDP discovery and port mapping
 * @version 3.1.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <span>
#include <expected>
#include <sstream>
#include <chrono>
#include <algorithm>

#include "udp_socket.hpp"
#include "socket_address.hpp"
#include "internet_protocol.hpp"
#include "dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  UPnP Constants
// ═══════════════════════════════════════════════

namespace upnp_detail {
	inline constexpr std::string_view SSDP_MULTICAST_ADDR = "239.255.255.250";
	inline constexpr uint16_t         SSDP_PORT           = 1900;
	inline constexpr std::string_view IGD_URN =
		"urn:schemas-upnp-org:device:InternetGatewayDevice:1";
	inline constexpr std::string_view WAN_IP_URN =
		"urn:schemas-upnp-org:service:WANIPConnection:1";
	inline constexpr std::string_view WAN_PPP_URN =
		"urn:schemas-upnp-org:service:WANPPPConnection:1";
} // namespace upnp_detail

// ═══════════════════════════════════════════════
//  Protocol Type for Port Mapping
// ═══════════════════════════════════════════════

enum class PortProtocol : uint8_t {
	TCP,
	UDP
};

inline constexpr std::string_view port_protocol_string(PortProtocol p) noexcept {
	return (p == PortProtocol::TCP) ? "TCP" : "UDP";
}

// ═══════════════════════════════════════════════
//  UPnP Device (discovered IGD)
// ═══════════════════════════════════════════════

/**
 * @brief A discovered UPnP Internet Gateway Device
 */
struct UpnpDevice {
	std::string location;      // HTTP URL to device XML descriptor
	std::string server;        // Server header value
	std::string usn;           // Unique service name
	std::string control_url;   // SOAP control URL (populated after XML fetch)
	std::string service_type;  // WANIPConnection or WANPPPConnection

	bool is_valid() const noexcept {
		return !location.empty();
	}
};

// ═══════════════════════════════════════════════
//  Port Mapping
// ═══════════════════════════════════════════════

/**
 * @brief A UPnP port mapping entry
 */
struct PortMapping {
	uint16_t     external_port = 0;
	uint16_t     internal_port = 0;
	std::string  internal_client;      // Internal IP address
	PortProtocol protocol       = PortProtocol::TCP;
	std::string  description    = "etherz";
	uint32_t     lease_duration = 0;   // 0 = permanent
	bool         enabled        = true;
};

// ═══════════════════════════════════════════════
//  SSDP Message Builder
// ═══════════════════════════════════════════════

namespace upnp_detail {

/**
 * @brief Build an SSDP M-SEARCH request
 */
inline std::string build_msearch(std::string_view search_target,
	uint8_t mx_seconds = 3)
{
	std::string msg;
	msg += "M-SEARCH * HTTP/1.1\r\n";
	msg += "HOST: 239.255.255.250:1900\r\n";
	msg += "MAN: \"ssdp:discover\"\r\n";
	msg += "MX: " + std::to_string(mx_seconds) + "\r\n";
	msg += "ST: " + std::string(search_target) + "\r\n";
	msg += "\r\n";
	return msg;
}

/**
 * @brief Parse SSDP response headers into a device struct
 */
inline UpnpDevice parse_ssdp_response(std::string_view response) {
	UpnpDevice device;

	auto get_header = [&](std::string_view name) -> std::string {
		// Case-insensitive header search
		std::string lower_resp(response);
		std::string lower_name(name);
		std::transform(lower_resp.begin(), lower_resp.end(),
			lower_resp.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(lower_name.begin(), lower_name.end(),
			lower_name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		auto pos = lower_resp.find(lower_name);
		if (pos == std::string::npos) return "";

		auto colon = response.find(':', pos + name.size());
		if (colon == std::string_view::npos) return "";

		auto start = colon + 1;
		while (start < response.size() && response[start] == ' ') ++start;

		auto end = response.find("\r\n", start);
		if (end == std::string_view::npos) end = response.size();

		return std::string(response.substr(start, end - start));
	};

	device.location = get_header("LOCATION");
	device.server   = get_header("SERVER");
	device.usn      = get_header("USN");

	return device;
}

/**
 * @brief Extract a value between XML tags (simple, non-recursive)
 */
inline std::string xml_extract(std::string_view xml, std::string_view tag) {
	std::string open_tag = "<" + std::string(tag) + ">";
	std::string close_tag = "</" + std::string(tag) + ">";

	auto start = xml.find(open_tag);
	if (start == std::string_view::npos) return "";
	start += open_tag.size();

	auto end = xml.find(close_tag, start);
	if (end == std::string_view::npos) return "";

	return std::string(xml.substr(start, end - start));
}

/**
 * @brief Build a SOAP action request envelope
 */
inline std::string build_soap_request(std::string_view service_type,
	std::string_view action, std::string_view arguments)
{
	std::string soap;
	soap += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n";
	soap += "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"";
	soap += " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n";
	soap += "<s:Body>\r\n";
	soap += "<u:" + std::string(action);
	soap += " xmlns:u=\"" + std::string(service_type) + "\">\r\n";
	soap += std::string(arguments);
	soap += "</u:" + std::string(action) + ">\r\n";
	soap += "</s:Body>\r\n";
	soap += "</s:Envelope>\r\n";
	return soap;
}

/**
 * @brief Build an HTTP POST for SOAP action
 */
inline std::string build_soap_http(std::string_view host, std::string_view path,
	std::string_view service_type, std::string_view action,
	std::string_view soap_body)
{
	std::string http;
	http += "POST " + std::string(path) + " HTTP/1.1\r\n";
	http += "HOST: " + std::string(host) + "\r\n";
	http += "Content-Type: text/xml; charset=\"utf-8\"\r\n";
	http += "Content-Length: " + std::to_string(soap_body.size()) + "\r\n";
	http += "SOAPACTION: \"" + std::string(service_type) + "#"
		+ std::string(action) + "\"\r\n";
	http += "\r\n";
	http += soap_body;
	return http;
}

/**
 * @brief Parse host and port from a URL
 */
inline std::pair<std::string, uint16_t> parse_url_host_port(std::string_view url) {
	// Skip scheme
	auto scheme_end = url.find("://");
	if (scheme_end != std::string_view::npos) {
		url = url.substr(scheme_end + 3);
	}

	// Extract host:port
	auto path_start = url.find('/');
	auto host_port = (path_start != std::string_view::npos)
		? url.substr(0, path_start)
		: url;

	auto colon = host_port.find(':');
	if (colon != std::string_view::npos) {
		auto host = std::string(host_port.substr(0, colon));
		uint16_t port = 0;
		auto port_str = host_port.substr(colon + 1);
		for (char c : port_str) {
			if (c >= '0' && c <= '9') {
				port = port * 10 + static_cast<uint16_t>(c - '0');
			}
		}
		return {host, port};
	}

	return {std::string(host_port), 80};
}

/**
 * @brief Extract URL path from a full URL
 */
inline std::string parse_url_path(std::string_view url) {
	auto scheme_end = url.find("://");
	if (scheme_end != std::string_view::npos) {
		url = url.substr(scheme_end + 3);
	}
	auto path_start = url.find('/');
	if (path_start != std::string_view::npos) {
		return std::string(url.substr(path_start));
	}
	return "/";
}

} // namespace upnp_detail

// ═══════════════════════════════════════════════
//  UPnP Client
// ═══════════════════════════════════════════════

/**
 * @brief UPnP IGD client for NAT traversal
 * 
 * Discovers Internet Gateway Devices on the local network via SSDP,
 * then uses UPnP SOAP actions to manage port mappings.
 * 
 * Usage:
 *   auto devices = UpnpClient::discover();
 *   if (!devices.empty()) {
 *       auto& gw = devices[0];
 *       UpnpClient::fetch_control_url(gw);
 *       
 *       PortMapping map;
 *       map.external_port = 8080;
 *       map.internal_port = 8080;
 *       map.internal_client = "192.168.1.100";
 *       map.protocol = PortProtocol::TCP;
 *       
 *       UpnpClient::add_port_mapping(gw, map);
 *       auto ext_ip = UpnpClient::get_external_ip(gw);
 *   }
 */
class UpnpClient {
public:
	/**
	 * @brief Discover UPnP IGD devices via SSDP M-SEARCH
	 * @param timeout_ms How long to wait for responses (default 3s)
	 * @return Vector of discovered devices
	 */
	static std::vector<UpnpDevice> discover(uint32_t timeout_ms = 3000) {
		std::vector<UpnpDevice> devices;

		UdpSocket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err)) return devices;
		sock.set_timeout(timeout_ms);

		auto multicast_addr = SocketAddress<Ip<4>>(
			Ip<4>(239, 255, 255, 250),
			upnp_detail::SSDP_PORT);

		// Send M-SEARCH for IGD devices
		auto msearch = upnp_detail::build_msearch(upnp_detail::IGD_URN);
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(msearch.data()), msearch.size());
		sock.send_to(data, multicast_addr);

		// Collect responses
		std::array<uint8_t, 2048> buffer{};
		while (true) {
			auto result = sock.recv_from(buffer);
			if (result.bytes_received <= 0) break;

			std::string_view response(
				reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(result.bytes_received));

			auto device = upnp_detail::parse_ssdp_response(response);
			if (device.is_valid()) {
				devices.push_back(std::move(device));
			}
		}

		return devices;
	}

	/**
	 * @brief Fetch the control URL from the device's XML descriptor
	 * @param device Device to populate control_url for
	 * @return Error on failure
	 */
	static core::Error fetch_control_url(UpnpDevice& device) {
		if (device.location.empty()) return core::Error::UpnpError;

		auto [host, port] = upnp_detail::parse_url_host_port(device.location);
		auto path = upnp_detail::parse_url_path(device.location);

		// Resolve host
		auto ip = Ip<4>(127, 0, 0, 1);
		auto dns = Dns::resolve(host);
		if (dns.success && !dns.ipv4_addresses.empty()) {
			ip = dns.ipv4_addresses[0];
		} else {
			ip = Ip<4>{host};
		}

		// Fetch the XML descriptor via TCP
		Socket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err)) return err;
		sock.set_timeout(5000);

		auto addr = SocketAddress<Ip<4>>(ip, port);
		if (auto err = sock.connect(addr); core::is_error(err)) return err;

		std::string request = "GET " + path + " HTTP/1.1\r\n";
		request += "Host: " + host + "\r\n";
		request += "Connection: close\r\n\r\n";

		auto req_data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(request.data()), request.size());
		sock.send(req_data);

		// Read response
		std::string xml_response;
		std::array<uint8_t, 4096> buf{};
		while (true) {
			int received = sock.recv(buf);
			if (received <= 0) break;
			xml_response.append(reinterpret_cast<const char*>(buf.data()),
				static_cast<size_t>(received));
		}

		// Try WANIPConnection first, then WANPPPConnection
		auto control = upnp_detail::xml_extract(xml_response, "controlURL");
		if (control.empty()) {
			return core::Error::UpnpError;
		}

		device.control_url = control;

		// Determine service type
		if (xml_response.find("WANPPPConnection") != std::string::npos) {
			device.service_type = std::string(upnp_detail::WAN_PPP_URN);
		} else {
			device.service_type = std::string(upnp_detail::WAN_IP_URN);
		}

		return core::Error::None;
	}

	/**
	 * @brief Add a port mapping on the gateway
	 * @param device The gateway device (must have control_url populated)
	 * @param mapping The port mapping to add
	 * @return Error on failure
	 */
	static core::Error add_port_mapping(const UpnpDevice& device,
		const PortMapping& mapping)
	{
		std::string args;
		args += "<NewRemoteHost></NewRemoteHost>\r\n";
		args += "<NewExternalPort>" + std::to_string(mapping.external_port)
			+ "</NewExternalPort>\r\n";
		args += "<NewProtocol>" + std::string(port_protocol_string(mapping.protocol))
			+ "</NewProtocol>\r\n";
		args += "<NewInternalPort>" + std::to_string(mapping.internal_port)
			+ "</NewInternalPort>\r\n";
		args += "<NewInternalClient>" + mapping.internal_client
			+ "</NewInternalClient>\r\n";
		args += "<NewEnabled>" + std::string(mapping.enabled ? "1" : "0")
			+ "</NewEnabled>\r\n";
		args += "<NewPortMappingDescription>" + mapping.description
			+ "</NewPortMappingDescription>\r\n";
		args += "<NewLeaseDuration>" + std::to_string(mapping.lease_duration)
			+ "</NewLeaseDuration>\r\n";

		return send_soap_action(device, "AddPortMapping", args);
	}

	/**
	 * @brief Delete a port mapping from the gateway
	 * @param device The gateway device
	 * @param external_port External port to unmap
	 * @param protocol TCP or UDP
	 * @return Error on failure
	 */
	static core::Error delete_port_mapping(const UpnpDevice& device,
		uint16_t external_port, PortProtocol protocol)
	{
		std::string args;
		args += "<NewRemoteHost></NewRemoteHost>\r\n";
		args += "<NewExternalPort>" + std::to_string(external_port)
			+ "</NewExternalPort>\r\n";
		args += "<NewProtocol>" + std::string(port_protocol_string(protocol))
			+ "</NewProtocol>\r\n";

		return send_soap_action(device, "DeletePortMapping", args);
	}

	/**
	 * @brief Get the external (public) IP address from the gateway
	 * @param device The gateway device
	 * @return External IP string, or error
	 */
	static std::expected<std::string, core::Error> get_external_ip(
		const UpnpDevice& device)
	{
		auto response = send_soap_action_with_response(
			device, "GetExternalIPAddress", "");

		if (!response.has_value()) {
			return std::unexpected(response.error());
		}

		auto ip = upnp_detail::xml_extract(*response, "NewExternalIPAddress");
		if (ip.empty()) {
			return std::unexpected(core::Error::UpnpError);
		}
		return ip;
	}

	// ─── SSDP message helpers (public for testing) ───

	/**
	 * @brief Build an SSDP M-SEARCH message
	 */
	static std::string build_msearch_message(
		std::string_view target = upnp_detail::IGD_URN,
		uint8_t mx = 3)
	{
		return upnp_detail::build_msearch(target, mx);
	}

	/**
	 * @brief Parse an SSDP response
	 */
	static UpnpDevice parse_ssdp(std::string_view response) {
		return upnp_detail::parse_ssdp_response(response);
	}

	/**
	 * @brief Build a SOAP port mapping request body
	 */
	static std::string build_add_mapping_soap(const UpnpDevice& device,
		const PortMapping& mapping)
	{
		std::string args;
		args += "<NewRemoteHost></NewRemoteHost>\r\n";
		args += "<NewExternalPort>" + std::to_string(mapping.external_port)
			+ "</NewExternalPort>\r\n";
		args += "<NewProtocol>" + std::string(port_protocol_string(mapping.protocol))
			+ "</NewProtocol>\r\n";
		args += "<NewInternalPort>" + std::to_string(mapping.internal_port)
			+ "</NewInternalPort>\r\n";
		args += "<NewInternalClient>" + mapping.internal_client
			+ "</NewInternalClient>\r\n";
		args += "<NewEnabled>" + std::string(mapping.enabled ? "1" : "0")
			+ "</NewEnabled>\r\n";
		args += "<NewPortMappingDescription>" + mapping.description
			+ "</NewPortMappingDescription>\r\n";
		args += "<NewLeaseDuration>" + std::to_string(mapping.lease_duration)
			+ "</NewLeaseDuration>\r\n";

		return upnp_detail::build_soap_request(
			device.service_type, "AddPortMapping", args);
	}

private:
	/**
	 * @brief Send a SOAP action to the device and check for success
	 */
	static core::Error send_soap_action(const UpnpDevice& device,
		std::string_view action, std::string_view arguments)
	{
		auto result = send_soap_action_with_response(device, action, arguments);
		if (!result.has_value()) return result.error();
		return core::Error::None;
	}

	/**
	 * @brief Send a SOAP action and return the response body
	 */
	static std::expected<std::string, core::Error> send_soap_action_with_response(
		const UpnpDevice& device, std::string_view action,
		std::string_view arguments)
	{
		if (device.control_url.empty() || device.service_type.empty()) {
			return std::unexpected(core::Error::UpnpError);
		}

		auto [host, port] = upnp_detail::parse_url_host_port(device.location);

		// Build the control URL path
		std::string control_path = device.control_url;
		if (!control_path.empty() && control_path[0] != '/') {
			control_path = "/" + control_path;
		}

		auto soap_body = upnp_detail::build_soap_request(
			device.service_type, action, arguments);

		auto http_req = upnp_detail::build_soap_http(
			host, control_path, device.service_type, action, soap_body);

		// Connect and send
		auto ip = Ip<4>(127, 0, 0, 1);
		auto dns = Dns::resolve(host);
		if (dns.success && !dns.ipv4_addresses.empty()) {
			ip = dns.ipv4_addresses[0];
		} else {
			ip = Ip<4>{host};
		}

		Socket<Ip<4>> sock;
		if (auto err = sock.create(); core::is_error(err))
			return std::unexpected(err);
		sock.set_timeout(5000);

		auto addr = SocketAddress<Ip<4>>(ip, port);
		if (auto err = sock.connect(addr); core::is_error(err))
			return std::unexpected(err);

		auto req_data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(http_req.data()), http_req.size());
		sock.send(req_data);

		// Read response
		std::string response;
		std::array<uint8_t, 4096> buf{};
		while (true) {
			int received = sock.recv(buf);
			if (received <= 0) break;
			response.append(reinterpret_cast<const char*>(buf.data()),
				static_cast<size_t>(received));
		}

		// Check HTTP status
		if (response.find("200 OK") == std::string::npos) {
			return std::unexpected(core::Error::UpnpError);
		}

		return response;
	}
};

} // namespace net
} // namespace etherz

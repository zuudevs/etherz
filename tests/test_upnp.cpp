/**
 * @file test_upnp.cpp
 * @brief Unit tests for UPnP — SSDP message parsing, port mapping requests, XML extraction
 */

#include "test_framework.hpp"
#include "net/upnp.hpp"

namespace etn = etherz::net;

// ═══════════════════════════════════════════════
//  SSDP Message Tests
// ═══════════════════════════════════════════════

TEST(upnp_msearch_format) {
	auto msg = etn::UpnpClient::build_msearch_message();
	ASSERT_TRUE(msg.find("M-SEARCH * HTTP/1.1") != std::string::npos);
	ASSERT_TRUE(msg.find("HOST: 239.255.255.250:1900") != std::string::npos);
	ASSERT_TRUE(msg.find("MAN: \"ssdp:discover\"") != std::string::npos);
	ASSERT_TRUE(msg.find("ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1")
		!= std::string::npos);
	ASSERT_TRUE(msg.find("MX: 3") != std::string::npos);
}

TEST(upnp_msearch_custom_mx) {
	auto msg = etn::UpnpClient::build_msearch_message(
		etn::upnp_detail::IGD_URN, 5);
	ASSERT_TRUE(msg.find("MX: 5") != std::string::npos);
}

TEST(upnp_msearch_custom_target) {
	auto msg = etn::UpnpClient::build_msearch_message("ssdp:all");
	ASSERT_TRUE(msg.find("ST: ssdp:all") != std::string::npos);
}

// ═══════════════════════════════════════════════
//  SSDP Response Parsing
// ═══════════════════════════════════════════════

TEST(upnp_parse_ssdp_response) {
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"LOCATION: http://192.168.1.1:5000/rootDesc.xml\r\n"
		"SERVER: Linux/3.14 UPnP/1.0 MiniUPnPd/1.9\r\n"
		"USN: uuid:abcd-1234::urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
		"\r\n";

	auto device = etn::UpnpClient::parse_ssdp(response);
	ASSERT_TRUE(device.is_valid());
	ASSERT_EQ(device.location, "http://192.168.1.1:5000/rootDesc.xml");
	ASSERT_EQ(device.server, "Linux/3.14 UPnP/1.0 MiniUPnPd/1.9");
	ASSERT_TRUE(device.usn.find("uuid:abcd-1234") != std::string::npos);
}

TEST(upnp_parse_ssdp_empty) {
	auto device = etn::UpnpClient::parse_ssdp("");
	ASSERT_TRUE(!device.is_valid());
	ASSERT_TRUE(device.location.empty());
}

TEST(upnp_parse_ssdp_no_location) {
	std::string response =
		"HTTP/1.1 200 OK\r\n"
		"SERVER: TestServer/1.0\r\n"
		"\r\n";

	auto device = etn::UpnpClient::parse_ssdp(response);
	ASSERT_TRUE(!device.is_valid());
}

// ═══════════════════════════════════════════════
//  XML Extract
// ═══════════════════════════════════════════════

TEST(upnp_xml_extract) {
	std::string xml = "<root><controlURL>/ctl/IPConn</controlURL></root>";
	auto url = etn::upnp_detail::xml_extract(xml, "controlURL");
	ASSERT_EQ(url, "/ctl/IPConn");
}

TEST(upnp_xml_extract_missing) {
	std::string xml = "<root><other>val</other></root>";
	auto url = etn::upnp_detail::xml_extract(xml, "controlURL");
	ASSERT_TRUE(url.empty());
}

TEST(upnp_xml_extract_nested) {
	std::string xml =
		"<device>"
		"<serviceList>"
		"<service>"
		"<controlURL>/ctl/WANIPConn</controlURL>"
		"</service>"
		"</serviceList>"
		"</device>";
	auto url = etn::upnp_detail::xml_extract(xml, "controlURL");
	ASSERT_EQ(url, "/ctl/WANIPConn");
}

// ═══════════════════════════════════════════════
//  Port Mapping SOAP Request
// ═══════════════════════════════════════════════

TEST(upnp_add_mapping_soap) {
	etn::UpnpDevice device;
	device.service_type = "urn:schemas-upnp-org:service:WANIPConnection:1";
	device.control_url = "/ctl/IPConn";

	etn::PortMapping mapping;
	mapping.external_port   = 8080;
	mapping.internal_port   = 80;
	mapping.internal_client = "192.168.1.100";
	mapping.protocol        = etn::PortProtocol::TCP;
	mapping.description     = "test-mapping";
	mapping.lease_duration  = 3600;

	auto soap = etn::UpnpClient::build_add_mapping_soap(device, mapping);
	ASSERT_TRUE(soap.find("AddPortMapping") != std::string::npos);
	ASSERT_TRUE(soap.find("<NewExternalPort>8080</NewExternalPort>") != std::string::npos);
	ASSERT_TRUE(soap.find("<NewInternalPort>80</NewInternalPort>") != std::string::npos);
	ASSERT_TRUE(soap.find("<NewInternalClient>192.168.1.100</NewInternalClient>") != std::string::npos);
	ASSERT_TRUE(soap.find("<NewProtocol>TCP</NewProtocol>") != std::string::npos);
	ASSERT_TRUE(soap.find("<NewLeaseDuration>3600</NewLeaseDuration>") != std::string::npos);
}

TEST(upnp_add_mapping_udp) {
	etn::UpnpDevice device;
	device.service_type = "urn:schemas-upnp-org:service:WANIPConnection:1";

	etn::PortMapping mapping;
	mapping.external_port   = 5000;
	mapping.internal_port   = 5000;
	mapping.internal_client = "10.0.0.5";
	mapping.protocol        = etn::PortProtocol::UDP;

	auto soap = etn::UpnpClient::build_add_mapping_soap(device, mapping);
	ASSERT_TRUE(soap.find("<NewProtocol>UDP</NewProtocol>") != std::string::npos);
}

// ═══════════════════════════════════════════════
//  URL Parsing
// ═══════════════════════════════════════════════

TEST(upnp_url_host_port) {
	auto [host, port] = etn::upnp_detail::parse_url_host_port(
		"http://192.168.1.1:5000/rootDesc.xml");
	ASSERT_EQ(host, "192.168.1.1");
	ASSERT_EQ(port, 5000);
}

TEST(upnp_url_host_default_port) {
	auto [host, port] = etn::upnp_detail::parse_url_host_port(
		"http://192.168.1.1/rootDesc.xml");
	ASSERT_EQ(host, "192.168.1.1");
	ASSERT_EQ(port, 80);
}

TEST(upnp_url_path) {
	auto path = etn::upnp_detail::parse_url_path(
		"http://192.168.1.1:5000/rootDesc.xml");
	ASSERT_EQ(path, "/rootDesc.xml");
}

TEST(upnp_url_path_no_path) {
	auto path = etn::upnp_detail::parse_url_path("http://192.168.1.1");
	ASSERT_EQ(path, "/");
}

// ═══════════════════════════════════════════════
//  PortProtocol
// ═══════════════════════════════════════════════

TEST(upnp_port_protocol_string) {
	ASSERT_EQ(etn::port_protocol_string(etn::PortProtocol::TCP), "TCP");
	ASSERT_EQ(etn::port_protocol_string(etn::PortProtocol::UDP), "UDP");
}

// ═══════════════════════════════════════════════
//  Device Struct
// ═══════════════════════════════════════════════

TEST(upnp_device_validity) {
	etn::UpnpDevice empty;
	ASSERT_TRUE(!empty.is_valid());

	etn::UpnpDevice valid;
	valid.location = "http://192.168.1.1:5000/root.xml";
	ASSERT_TRUE(valid.is_valid());
}

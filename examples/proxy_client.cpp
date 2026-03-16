/**
 * @file proxy_client.cpp
 * @brief Example: HTTP GET through a SOCKS5 or HTTP CONNECT proxy
 * 
 * Usage:
 *   proxy_client socks5 <proxy_host> <proxy_port> <url>
 *   proxy_client http   <proxy_host> <proxy_port> <url>
 */

#include "../include/net/proxy.hpp"
#include "../include/protocol/http_client.hpp"
#include "../include/protocol/url.hpp"
#include <print>
#include <string>

namespace en = etherz::net;
namespace ep = etherz::protocol;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	if (argc < 5) {
		std::print("Usage: proxy_client <socks5|http> <proxy_host> <proxy_port> <url>\n");
		return 1;
	}

	std::string proxy_type = argv[1];
	std::string proxy_host = argv[2];
	uint16_t proxy_port = static_cast<uint16_t>(std::stoi(argv[3]));
	std::string url_str = argv[4];

	auto url = ep::Url::parse(url_str);
	auto target_ip = en::Ip<4>(127, 0, 0, 1);
	auto dns = en::Dns::resolve(url.host);
	if (dns.success && !dns.ipv4_addresses.empty()) {
		target_ip = dns.ipv4_addresses[0];
	}

	auto target = en::SocketAddress<en::Ip<4>>(target_ip, url.port);

	en::ProxyConfig proxy;
	if (proxy_type == "socks5") {
		proxy = en::ProxyConfig::socks5(proxy_host, proxy_port);
	} else {
		proxy = en::ProxyConfig::http_connect(proxy_host, proxy_port);
	}

	std::print("Connecting to {} via {} proxy {}:{}\n",
		url_str, proxy_type, proxy_host, proxy_port);

	auto result = en::connect_through_proxy(target, url.host, proxy);
	if (!result) {
		std::print("Proxy connect failed: {}\n", ec::error_message(result.error()));
		return 1;
	}

	// Send HTTP request through the tunneled connection
	auto& sock = *result;
	ep::HttpRequest req;
	req.method = ep::HttpMethod::Get;
	req.path = url.path.empty() ? "/" : url.path;
	req.headers.set("Host", url.host);
	req.headers.set("Connection", "close");
	req.headers.set("User-Agent", "Etherz/1.2.0");

	auto raw = req.serialize();
	auto data = std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
	sock.send(data);

	// Receive response
	std::string response_data;
	std::array<uint8_t, 4096> buffer{};
	while (true) {
		int received = sock.recv(buffer);
		if (received <= 0) break;
		response_data.append(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));
	}

	auto resp = ep::http_parser::parse_response(response_data);
	std::print("Status: {}\n", static_cast<uint16_t>(resp.status));
	std::print("Body ({} bytes):\n{}\n", resp.body.size(),
		resp.body.substr(0, 500));

	return 0;
}

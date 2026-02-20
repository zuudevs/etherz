/**
 * @file socket_address.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Socket Address implementation for IPv4 and IPv6
 * @version 1.0.0
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <type_traits>
#include "internet_protocol.hpp"

namespace etherz {
namespace net {

template <typename T>
class SocketAddress {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>, "Invalid IP version.");
};

template <>
class SocketAddress<Ip<4>> {
public:
	using protocol_type = Ip<4>;
	using port_type = uint16_t;

	constexpr SocketAddress() noexcept = default;
	constexpr SocketAddress(const SocketAddress&) noexcept = default;
	constexpr SocketAddress(SocketAddress&&) noexcept = default;
	constexpr SocketAddress& operator=(const SocketAddress&) noexcept = default;
	constexpr SocketAddress& operator=(SocketAddress&&) noexcept = default;
	constexpr auto operator<=>(const SocketAddress&) const noexcept = default;
	constexpr ~SocketAddress() noexcept = default;

	constexpr SocketAddress(protocol_type addr, port_type p) noexcept 
		: address_(addr), port_(p) {}

	constexpr SocketAddress(std::string_view addr, std::string_view port) noexcept 
		: address_(addr) 
	{
		if (port.empty() || port.size() > 5) return;
		uint32_t port_v = 0;
		for (size_t i = 0; i < port.size(); ++i) {
			if (port[i] < '0' || port[i] > '9') {
				port_ = 0;
				return;
			}
			port_v = port_v * 10 + static_cast<uint32_t>(port[i] - '0');
		}
		if (port_v > 65535) {
			port_ = 0;
			return;
		}
		port_ = static_cast<port_type>(port_v);
	}

	// Accessors
	constexpr const protocol_type& address() const noexcept { return address_; }
	constexpr protocol_type& address() noexcept { return address_; }
	constexpr port_type port() const noexcept { return port_; }
	constexpr void set_port(port_type p) noexcept { port_ = p; }
	constexpr void set_address(protocol_type addr) noexcept { address_ = addr; }

	inline void display() const noexcept {
		std::print("SocketAddress IPv4: {}.{}.{}.{}:{}\n",
			address_.bytes()[0], address_.bytes()[1],
			address_.bytes()[2], address_.bytes()[3], port_);
	}

private:
	protocol_type address_{};
	port_type port_{};
};

template <>
class SocketAddress<Ip<6>> {
public:
	using protocol_type = Ip<6>;
	using port_type = uint16_t;

	constexpr SocketAddress() noexcept = default;
	constexpr SocketAddress(const SocketAddress&) noexcept = default;
	constexpr SocketAddress(SocketAddress&&) noexcept = default;
	constexpr SocketAddress& operator=(const SocketAddress&) noexcept = default;
	constexpr SocketAddress& operator=(SocketAddress&&) noexcept = default;
	constexpr auto operator<=>(const SocketAddress&) const noexcept = default;
	constexpr ~SocketAddress() noexcept = default;

	constexpr SocketAddress(protocol_type addr, port_type p) noexcept 
		: address_(addr), port_(p) {}

	// Accessors
	constexpr const protocol_type& address() const noexcept { return address_; }
	constexpr protocol_type& address() noexcept { return address_; }
	constexpr port_type port() const noexcept { return port_; }
	constexpr void set_port(port_type p) noexcept { port_ = p; }
	constexpr void set_address(protocol_type addr) noexcept { address_ = addr; }

	inline void display() const noexcept {
		std::print("SocketAddress IPv6: [{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}]:{}\n",
			address_.bytes()[0], address_.bytes()[1],
			address_.bytes()[2], address_.bytes()[3],
			address_.bytes()[4], address_.bytes()[5],
			address_.bytes()[6], address_.bytes()[7], port_);
	}

private:
	protocol_type address_{};
	port_type port_{};
};

} // namespace net
} // namespace etherz
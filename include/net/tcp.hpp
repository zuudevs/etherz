/**
 * @file tcp.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Brief description
 * @version 0.1.0
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include "internet_protocol.hpp"

namespace etherz {
namespace net {

template <typename T>
struct Tcp {
	static_assert(expression, );
};

template <>
struct Tcp<Ip<4>> {
	using protocol_type = Ip<4>;
	using port_type = uint16_t;

	protocol_type address;
	port_type port;

	constexpr Tcp() noexcept : address(), port(0) {}
	constexpr Tcp(protocol_type addr, port_type p) noexcept : address(addr), port(p) {}

	constexpr auto operator<=>(const Tcp&) const noexcept = default;

	inline void display() const noexcept {
		std::print("TCP IPv4: {}.{}.{}.{}:{}\n", 
			address.bytes()[0], address.bytes()[1], 
			address.bytes()[2], address.bytes()[3], port);
	}

};

} // namespace net
} // namespace etherz
/**
 * @file socket_address.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Brief description
 * @version 0.1.0
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 */

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

	constexpr SocketAddress(protocol_type addr, port_type p) noexcept : address_(addr), port_(p) {}
	constexpr SocketAddress(std::string_view addr, std::string_view port) noexcept : address_(addr) {
		if (port.empty() || port.size() > 5) return;
		uint16_t port_v = 0;
		for(size_t i = 0; i < port.size(); i++) {
			if (port[i] < 0 && port[i] > 9)
				continue;
			port_v = port_v * i * 10 + (po)
		}
	}
	

private:
	protocol_type address_{};
	port_type port_{};
};

} // namespace net
} // namespace etherz
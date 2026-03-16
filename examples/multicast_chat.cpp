/**
 * @file multicast_chat.cpp
 * @brief Multicast chat example — send/receive messages on a multicast group
 * 
 * Usage:
 *   multicast_chat send <message>    — Send a message to the group
 *   multicast_chat recv              — Listen for messages
 */

#include "../include/net/multicast_socket.hpp"
#include "../include/net/internet_protocol.hpp"
#include "../include/net/socket_address.hpp"
#include <print>
#include <string>
#include <array>

namespace en = etherz::net;

constexpr auto GROUP = en::Ip<4>(239, 1, 1, 1);   // Multicast group
constexpr uint16_t PORT = 5007;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: multicast_chat <send <msg> | recv>\n");
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "send" && argc >= 3) {
		// ─── Sender ─────────────────────
		en::MulticastSocket<en::Ip<4>> sock;
		if (auto err = sock.create(); etherz::core::is_error(err)) {
			std::print("Create failed: {}\n", etherz::core::error_message(err));
			return 1;
		}

		// Set TTL (1 = local subnet only)
		sock.set_ttl(1);

		std::string msg = argv[2];
		auto dest = en::SocketAddress<en::Ip<4>>(GROUP, PORT);
		auto data = std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(msg.data()), msg.size());

		int sent = sock.send_to(data, dest);
		std::print("Sent {} bytes to {}:{}\n", sent, GROUP.display(), PORT);
	}
	else if (mode == "recv") {
		// ─── Receiver ───────────────────
		en::MulticastSocket<en::Ip<4>> sock;
		if (auto err = sock.create(); etherz::core::is_error(err)) {
			std::print("Create failed: {}\n", etherz::core::error_message(err));
			return 1;
		}

		sock.set_reuse_addr(true);

		auto bind_addr = en::SocketAddress<en::Ip<4>>(en::Ip<4>(0, 0, 0, 0), PORT);
		if (auto err = sock.bind(bind_addr); etherz::core::is_error(err)) {
			std::print("Bind failed: {}\n", etherz::core::error_message(err));
			return 1;
		}

		if (auto err = sock.join_group(GROUP); etherz::core::is_error(err)) {
			std::print("Join group failed: {}\n", etherz::core::error_message(err));
			return 1;
		}

		std::print("Listening on multicast group {}:{} ...\n", GROUP.display(), PORT);

		std::array<uint8_t, 1024> buffer{};
		while (true) {
			auto result = sock.recv_from(buffer);
			if (result.bytes > 0) {
				std::string msg(reinterpret_cast<const char*>(buffer.data()),
					static_cast<size_t>(result.bytes));
				std::print("[{}] {}\n", result.sender.address().display(), msg);
			}
		}

		sock.leave_group(GROUP);
	}
	else {
		std::print("Usage: multicast_chat <send <msg> | recv>\n");
		return 1;
	}

	return 0;
}

#include "etherz.hpp"
#include "net/internet_protocol.hpp"
#include "net/socket_address.hpp"
#include "net/tcp.hpp"
#include "core/error.hpp"
#include <print>
#include <windows.h>

namespace etn = etherz::net;
namespace etc = etherz::core;

/**
 * @brief Initialize console to properly handle UTF-8 output.
 *
 * This function configures the console so UTF-8 encoded text
 * (e.g. Unicode characters, emojis, symbols) can be displayed correctly.
 *
 * Platform behavior:
 * - Windows:
 *   - Sets input and output code page to UTF-8 (CP_UTF8).
 *   - Enables full buffering for stdout to reduce flickering
 *     and improve performance.
 *
 * - Linux / macOS:
 *   - No action is required.
 *   - Modern terminals already use UTF-8 by default.
 *
 * Safe to call multiple times.
 */
inline void utf8_console() {
#if defined(_WIN32)
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	setvbuf(stdout, nullptr, _IOFBF, 1024);

#elif defined(__linux__) || defined(__APPLE__)
	(void)0;
#else
	(void)0;
#endif
}

int main() {
	utf8_console();
	std::print("═══════════════════════════════════\n");
	std::print("  Etherz v{} by {}\n", etherz::version(), etherz::author());
	std::print("═══════════════════════════════════\n\n");

	// ─── IPv4 ───────────────────────────
	std::print("── IPv4 ──────────────────────────\n");

	auto ip4a = etn::Ip(192, 168, 1, 50);
	auto ip4b = etn::Ip<4>{"10.0.0.1"};
	auto ip4c = etn::Ip<4>(0xC0A80101); // 192.168.1.1

	ip4a.display();
	ip4b.display();
	ip4c.display();

	// Arithmetic
	auto ip4d = ip4c + 5;
	std::print("192.168.1.1 + 5 = ");
	ip4d.display();
	++ip4d;
	std::print("After ++     = ");
	ip4d.display();

	// Network byte order
	std::print("Network order of 192.168.1.1: 0x{:08X}\n\n", ip4c.to_network());

	// ─── IPv6 ───────────────────────────
	std::print("── IPv6 ──────────────────────────\n");

	auto ip6a = etn::Ip<6>(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1);
	auto ip6b = etn::Ip<6>{"2001:0db8::1"};
	auto ip6c = etn::Ip<6>{"fe80::1"};
	auto ip6d = etn::Ip<6>{"2001:0db8:85a3:0000:0000:8a2e:0370:7334"};

	ip6a.display();
	ip6b.display();
	ip6c.display();
	ip6d.display();

	++ip6c;
	std::print("After ++ fe80::1 = ");
	ip6c.display();
	std::print("\n");

	// ─── SocketAddress ──────────────────
	std::print("── SocketAddress ─────────────────\n");

	auto sa4 = etn::SocketAddress<etn::Ip<4>>(etn::Ip(127, 0, 0, 1), 8080);
	auto sa4b = etn::SocketAddress<etn::Ip<4>>(etn::Ip<4>{"192.168.1.100"}, 443);
	auto sa4c = etn::SocketAddress<etn::Ip<4>>("0.0.0.0", "3000");

	sa4.display();
	sa4b.display();
	sa4c.display();

	auto sa6 = etn::SocketAddress<etn::Ip<6>>(
		etn::Ip<6>(0, 0, 0, 0, 0, 0, 0, 1), 8080
	);
	sa6.display();
	std::print("\n");

	// ─── TCP Endpoints ──────────────────
	std::print("── TCP Endpoints ─────────────────\n");

	auto tcp4 = etn::Tcp<etn::Ip<4>>(etn::Ip(10, 0, 0, 1), 80);
	auto tcp6 = etn::Tcp<etn::Ip<6>>(etn::Ip<6>{"::1"}, 443);

	tcp4.display();
	tcp6.display();
	std::print("\n");

	// ─── Error Types ────────────────────
	std::print("── Error Types ───────────────────\n");

	auto err1 = etc::Error::None;
	auto err2 = etc::Error::ConnectionRefused;

	std::print("{}: {} (ok={})\n", 
		static_cast<int>(err1), etc::error_message(err1), etc::is_ok(err1));
	std::print("{}: {} (ok={})\n", 
		static_cast<int>(err2), etc::error_message(err2), etc::is_ok(err2));
	std::print("\n");

	// ─── Comparison ─────────────────────
	std::print("── Comparison ────────────────────\n");

	auto cmp1 = etn::Ip(192, 168, 1, 1);
	auto cmp2 = etn::Ip(192, 168, 1, 2);
	std::print("192.168.1.1 == 192.168.1.2 ? {}\n", cmp1 == cmp2);
	std::print("192.168.1.1 <  192.168.1.2 ? {}\n", cmp1 < cmp2);
	std::print("192.168.1.1 == 192.168.1.1 ? {}\n", cmp1 == cmp1);

	std::print("\n═══════════════════════════════════\n");
	std::print("  All demos completed successfully!\n");
	std::print("═══════════════════════════════════\n");

	return 0;
}
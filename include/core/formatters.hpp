/**
 * @file formatters.hpp
 * @description std::formatter specializations for Etherz types
 */

#pragma once

#include <format>
#include <string_view>
#include "error.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"

// ─── Core Formatters ───────────────────────────

template <>
struct std::formatter<etherz::core::Error> : std::formatter<std::string_view> {
	auto format(etherz::core::Error err, std::format_context& ctx) const {
		return std::formatter<std::string_view>::format(etherz::core::error_message(err), ctx);
	}
};

// ─── Network Formatters ────────────────────────

template <>
struct std::formatter<etherz::net::Ip<4>> : std::formatter<std::string_view> {
	auto format(const etherz::net::Ip<4>& ip, std::format_context& ctx) const {
		auto b = ip.bytes();
		return std::format_to(ctx.out(), "{}.{}.{}.{}", b[0], b[1], b[2], b[3]);
	}
};

template <>
struct std::formatter<etherz::net::Ip<6>> : std::formatter<std::string_view> {
	auto format(const etherz::net::Ip<6>& ip, std::format_context& ctx) const {
		auto b = ip.bytes();
		return std::format_to(ctx.out(), "[{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}:{:04x}]",
			b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
	}
};

template <typename T>
struct std::formatter<etherz::net::SocketAddress<T>> : std::formatter<std::string_view> {
	auto format(const etherz::net::SocketAddress<T>& addr, std::format_context& ctx) const {
		return std::format_to(ctx.out(), "{}:{}", addr.address(), addr.port());
	}
};


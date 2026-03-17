/**
 * @file etherz.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Modern C++23 Header-Only Networking Library
 * @version 4.0.0
 * @date 2026-03-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string_view>
#include "core/formatters.hpp"

namespace etherz {
inline consteval uint8_t major() noexcept { return 4; }
inline consteval uint8_t minor() noexcept { return 0; }
inline consteval uint8_t patch() noexcept { return 0; }
inline consteval std::string_view version() noexcept { return "4.0.0"; }
inline consteval std::string_view author() noexcept { return "zuudevs"; }
inline consteval std::string_view email() noexcept { return "zuudevs@gmail.com"; }
inline consteval std::string_view github() noexcept { return "https://github.com/zuudevs"; }
inline consteval std::string_view repository() noexcept { return "https://github.com/zuudevs/etherz"; }
} // namespace etherz
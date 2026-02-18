/**
 * @file etherz.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Brief description
 * @version 0.1.0
 * @date 2026-02-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace etherz {
inline consteval uint8_t major() noexcept { return 0; }
inline consteval uint8_t minor() noexcept { return 1; }
inline consteval uint8_t patch() noexcept { return 0; }
inline consteval std::string_view version() noexcept { return "0.1.0"; }
inline consteval std::string_view author() noexcept { return "zuudevs"; }
inline consteval std::string_view email() noexcept { return "zuudevs@gmail.com"; }
inline consteval std::string_view github() noexcept { return "https://github.com/zuudevs"; }
inline consteval std::string_view repository() noexcept { return "https://github.com/zuudevs/etherz"; }
} // namespace etherz
/**
 * @file internet_protocol.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Internet Protocol Class Implementation
 * @version 0.1.4
 * @date 2026-02-17
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstdint>
#include <bit>
#include <print>
#include <string_view>

namespace etherz {
namespace net {

namespace impl {

/**
 * @brief Base class for IP implementations using CRTP.
 * 
 * @tparam Derived The derived class (Ip<4> etc)
 * @tparam T The element type (uint8_t, uint16_t, etc)
 * @tparam N The number of elements (4, 8, 16)
 */
template <typename Derived, typename T, size_t N>
class IpBase {
public:
    using value_type = T;
    static constexpr size_t length = N;

    constexpr IpBase() noexcept = default;
    constexpr IpBase(const IpBase&) noexcept = default;
    constexpr IpBase(IpBase&&) noexcept = default;
    constexpr IpBase& operator=(const IpBase&) noexcept = default;
    constexpr IpBase& operator=(IpBase&&) noexcept = default;
    constexpr auto operator<=>(const IpBase&) const noexcept = default;
    constexpr ~IpBase() noexcept = default;
    constexpr const std::array<T, N>& bytes() const noexcept { return m_address; }

protected:
    constexpr const Derived& derived() const noexcept { return static_cast<const Derived&>(*this); }
    constexpr Derived& derived() noexcept { return static_cast<Derived&>(*this); }
	
    std::array<T, N> m_address{};
};

} // namespace impl

/**
 * @brief Internet Protocol implementation declaration
 */
template <uint8_t Ipv>
class Ip {
    static_assert(Ipv == 4 || Ipv == 6, "Invalid IP version.");
};

/**
 * @brief IPv4 Specialization
 */
template <>
class Ip<4> : public impl::IpBase<Ip<4>, uint8_t, 4> {
public:
    static constexpr uint8_t version = 4;
    using Base = impl::IpBase<Ip<4>, uint8_t, 4>;

    // Constructors
    constexpr Ip() noexcept = default;
    constexpr Ip(const Ip&) noexcept = default;
    constexpr Ip(Ip&&) noexcept = default;
    constexpr Ip& operator=(const Ip&) noexcept = default;
    constexpr Ip& operator=(Ip&&) noexcept = default;
    constexpr auto operator<=>(const Ip&) const noexcept = default;
    constexpr ~Ip() noexcept = default;

    explicit constexpr Ip(uint32_t val) noexcept { from_uint32(val); }

    constexpr Ip(
		std::integral auto a, 
		std::integral auto b, 
		std::integral auto c, 
		std::integral auto d
	) noexcept {
        m_address = {
			static_cast<value_type>(a < decltype(a){0} ? decltype(a){0} : a), 
			static_cast<value_type>(b < decltype(b){0} ? decltype(b){0} : b), 
			static_cast<value_type>(c < decltype(c){0} ? decltype(c){0} : c), 
			static_cast<value_type>(d < decltype(d){0} ? decltype(d){0} : d)
		};
    }

    constexpr Ip(uint8_t (&arr)[4]) noexcept {
        std::copy(std::begin(arr), std::end(arr), m_address.begin());
    }

    /**
     * @brief Parse IP from string view.
     */
    constexpr Ip(std::string_view str) noexcept {
        if (str.empty() || str.size() > 15) return; // Fail fast

        size_t start = 0;
        int octet_idx = 0;
        
        for (size_t i = 0; i <= str.size(); ++i) {
            if (i == str.size() || str[i] == '.') {
                if (start == i) { fill(0); return; } 
                
                uint32_t val = 0;
                for (size_t k = start; k < i; ++k) {
                    char c = str[k];
                    if (c < '0' || c > '9') { fill(0); return; }
                    val = val * 10 + (c - '0');
                }

                if (val > 255) { fill(0); return; }

                if (octet_idx < 4) {
                    m_address[octet_idx++] = static_cast<uint8_t>(val);
                } else {
                    fill(0); return;
                }

                start = i + 1;
            }
        }

        if (octet_idx != 4) fill(0);
    }

    // Arithmetic Operators
    constexpr Ip operator+(uint32_t val) const noexcept { return Ip(to_uint32() + val); }
    constexpr Ip operator-(uint32_t val) const noexcept { return Ip(to_uint32() - val); }

    constexpr Ip& operator+=(uint32_t n) noexcept { 
        from_uint32(to_uint32() + n); 
        return *this;
    }

    constexpr Ip& operator-=(uint32_t n) noexcept { 
        from_uint32(to_uint32() - n); 
        return *this;
    }

    constexpr Ip& operator++() noexcept { return (*this += 1); }
    constexpr Ip& operator--() noexcept { return (*this -= 1); }

    constexpr Ip operator++(int) noexcept {
        auto rt = *this;
        ++*this;
        return rt; 
    }

    constexpr Ip operator--(int) noexcept {
        auto rt = *this;
        --*this;
        return rt; 
    }

    // Converters
    constexpr void from_uint32(uint32_t val) noexcept {
        m_address[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
        m_address[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
        m_address[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
        m_address[3] = static_cast<uint8_t>((val) & 0xFF);
    }

    constexpr uint32_t to_uint32() const noexcept {
        return (static_cast<uint32_t>(m_address[0]) << 24) | 
               (static_cast<uint32_t>(m_address[1]) << 16) | 
               (static_cast<uint32_t>(m_address[2]) << 8) | 
               (static_cast<uint32_t>(m_address[3]));
    }

    constexpr uint32_t to_network() const noexcept {
        if constexpr (std::endian::native == std::endian::big) {
            return to_uint32();
        } else {
            return std::byteswap(to_uint32());
        }
    }

    /**
     * @brief Fills the IP address with a value (0 to reset).
     * Requires <concepts>
     */
    constexpr void fill(std::integral auto val = 0) noexcept {
        auto n_val = static_cast<uint8_t>(val);
        // m_address is accessible because it's protected in Base
        m_address = {n_val, n_val, n_val, n_val};
    }

    inline void display() const noexcept {
        std::print("IPv4: {}.{}.{}.{}\n", 
            m_address[0], m_address[1], m_address[2], m_address[3]);
    }
};

template <typename A, typename B, typename C, typename D>
Ip(A, B, C, D) -> Ip<4>;

template <>
class Ip<6> {
    // TODO: Implement IPv6 using impl::IpBase<Ip<6>, uint16_t, 8> maybe?
};

} // namespace net
} // namespace etherz
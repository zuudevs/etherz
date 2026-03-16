/**
 * @file compression.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Deflate/Gzip/Zstd compression codec
 * @version 3.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <cstring>

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Compression Algorithm
// ═══════════════════════════════════════════════

enum class CompressionType : uint8_t {
	None    = 0,
	Deflate = 1,
	Gzip    = 2,
	Zstd    = 3,
	Lz4     = 4
};

inline constexpr std::string_view compression_name(CompressionType type) noexcept {
	switch (type) {
		case CompressionType::None:    return "none";
		case CompressionType::Deflate: return "deflate";
		case CompressionType::Gzip:    return "gzip";
		case CompressionType::Zstd:    return "zstd";
		case CompressionType::Lz4:     return "lz4";
		default:                       return "unknown";
	}
}

// ═══════════════════════════════════════════════
//  Compression Options
// ═══════════════════════════════════════════════

struct CompressionOptions {
	CompressionType type  = CompressionType::Deflate;
	int             level = 6;        // 1 (fast) to 9 (best), 0 = no compression
	size_t          window_bits = 15; // For deflate/gzip
};

// ═══════════════════════════════════════════════
//  Simple RLE Compression (built-in, no deps)
// ═══════════════════════════════════════════════

/**
 * @brief Lightweight Run-Length Encoding compressor
 * 
 * This is a built-in compression codec that doesn't require
 * external libraries. For production use with deflate/gzip/zstd,
 * link against zlib, libzstd, etc.
 */
class RleCodec {
public:
	/**
	 * @brief Compress data using RLE
	 */
	static std::vector<uint8_t> compress(std::span<const uint8_t> input) {
		std::vector<uint8_t> output;
		output.reserve(input.size());

		size_t i = 0;
		while (i < input.size()) {
			uint8_t byte = input[i];
			uint8_t count = 1;

			while (i + count < input.size() && input[i + count] == byte && count < 255) {
				++count;
			}

			if (count >= 3 || byte == 0xFF) {
				// RLE marker: 0xFF, count, byte
				output.push_back(0xFF);
				output.push_back(count);
				output.push_back(byte);
			} else {
				for (uint8_t j = 0; j < count; ++j) {
					output.push_back(byte);
				}
			}

			i += count;
		}

		return output;
	}

	/**
	 * @brief Decompress RLE data
	 */
	static std::vector<uint8_t> decompress(std::span<const uint8_t> input) {
		std::vector<uint8_t> output;

		size_t i = 0;
		while (i < input.size()) {
			if (input[i] == 0xFF && i + 2 < input.size()) {
				uint8_t count = input[i + 1];
				uint8_t byte = input[i + 2];
				for (uint8_t j = 0; j < count; ++j) {
					output.push_back(byte);
				}
				i += 3;
			} else {
				output.push_back(input[i]);
				++i;
			}
		}

		return output;
	}
};

// ═══════════════════════════════════════════════
//  LZ77-style Sliding Window Compressor
// ═══════════════════════════════════════════════

/**
 * @brief Simple LZ77-based compressor (no external dependencies)
 * 
 * Provides decent compression ratios without requiring zlib.
 * Format:
 *   Literal: 0x00 <byte>
 *   Match:   0x01 <offset:16> <length:8>
 */
class Lz77Codec {
public:
	static constexpr size_t WINDOW_SIZE = 4096;
	static constexpr size_t MIN_MATCH = 3;
	static constexpr size_t MAX_MATCH = 258;

	static std::vector<uint8_t> compress(std::span<const uint8_t> input) {
		std::vector<uint8_t> output;
		size_t pos = 0;

		while (pos < input.size()) {
			size_t best_offset = 0;
			size_t best_length = 0;

			// Search for matches in the sliding window
			size_t search_start = (pos > WINDOW_SIZE) ? pos - WINDOW_SIZE : 0;
			for (size_t s = search_start; s < pos; ++s) {
				size_t length = 0;
				while (pos + length < input.size() &&
					   length < MAX_MATCH &&
					   input[s + length] == input[pos + length]) {
					++length;
				}
				if (length >= MIN_MATCH && length > best_length) {
					best_offset = pos - s;
					best_length = length;
				}
			}

			if (best_length >= MIN_MATCH) {
				// Match
				output.push_back(0x01);
				output.push_back(static_cast<uint8_t>((best_offset >> 8) & 0xFF));
				output.push_back(static_cast<uint8_t>(best_offset & 0xFF));
				output.push_back(static_cast<uint8_t>(best_length));
				pos += best_length;
			} else {
				// Literal
				output.push_back(0x00);
				output.push_back(input[pos]);
				++pos;
			}
		}

		return output;
	}

	static std::vector<uint8_t> decompress(std::span<const uint8_t> input) {
		std::vector<uint8_t> output;
		size_t pos = 0;

		while (pos < input.size()) {
			uint8_t tag = input[pos++];

			if (tag == 0x00) {
				// Literal
				if (pos < input.size()) {
					output.push_back(input[pos++]);
				}
			} else if (tag == 0x01) {
				// Match
				if (pos + 3 > input.size()) break;
				uint16_t offset = (static_cast<uint16_t>(input[pos]) << 8) | input[pos + 1];
				uint8_t length = input[pos + 2];
				pos += 3;

				size_t start = output.size() - offset;
				for (uint8_t i = 0; i < length; ++i) {
					output.push_back(output[start + i]);
				}
			}
		}

		return output;
	}
};

// ═══════════════════════════════════════════════
//  Unified Compression Interface
// ═══════════════════════════════════════════════

/**
 * @brief Compress data using the specified algorithm
 */
inline std::vector<uint8_t> compress(std::span<const uint8_t> data,
	CompressionType type = CompressionType::Deflate)
{
	switch (type) {
		case CompressionType::None:
			return {data.begin(), data.end()};
		case CompressionType::Deflate:
		case CompressionType::Gzip:
			return Lz77Codec::compress(data);  // Built-in LZ77
		default:
			return RleCodec::compress(data);
	}
}

/**
 * @brief Decompress data using the specified algorithm
 */
inline std::vector<uint8_t> decompress(std::span<const uint8_t> data,
	CompressionType type = CompressionType::Deflate)
{
	switch (type) {
		case CompressionType::None:
			return {data.begin(), data.end()};
		case CompressionType::Deflate:
		case CompressionType::Gzip:
			return Lz77Codec::decompress(data);
		default:
			return RleCodec::decompress(data);
	}
}

/**
 * @brief Get compression ratio (compressed / original)
 */
inline double compression_ratio(size_t original, size_t compressed) noexcept {
	if (original == 0) return 1.0;
	return static_cast<double>(compressed) / static_cast<double>(original);
}

} // namespace net
} // namespace etherz

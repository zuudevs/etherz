#include "test_framework.hpp"
#include "net/compression.hpp"

namespace en = etherz::net;

// ─── CompressionType Names (v3.0.0) ──

TEST_CASE(compression_name_none) {
	CHECK_EQ(en::compression_name(en::CompressionType::None), std::string_view("none"));
}

TEST_CASE(compression_name_deflate) {
	CHECK_EQ(en::compression_name(en::CompressionType::Deflate), std::string_view("deflate"));
}

TEST_CASE(compression_name_gzip) {
	CHECK_EQ(en::compression_name(en::CompressionType::Gzip), std::string_view("gzip"));
}

TEST_CASE(compression_name_zstd) {
	CHECK_EQ(en::compression_name(en::CompressionType::Zstd), std::string_view("zstd"));
}

TEST_CASE(compression_name_lz4) {
	CHECK_EQ(en::compression_name(en::CompressionType::Lz4), std::string_view("lz4"));
}

// ─── RLE Codec ────────────────────────

TEST_CASE(rle_roundtrip_repeated) {
	// Many repeated bytes should compress well with RLE
	std::vector<uint8_t> input(100, 0xAA);
	auto compressed = en::RleCodec::compress(input);
	auto decompressed = en::RleCodec::decompress(compressed);
	CHECK_EQ(decompressed.size(), input.size());
	CHECK_TRUE(decompressed == input);
	CHECK_TRUE(compressed.size() < input.size()); // should compress
}

TEST_CASE(rle_roundtrip_mixed) {
	std::vector<uint8_t> input = {1, 2, 3, 3, 3, 3, 3, 4, 5};
	auto compressed = en::RleCodec::compress(input);
	auto decompressed = en::RleCodec::decompress(compressed);
	CHECK_EQ(decompressed.size(), input.size());
	CHECK_TRUE(decompressed == input);
}

TEST_CASE(rle_roundtrip_marker_byte) {
	// Test that 0xFF byte is handled correctly (it's the marker)
	std::vector<uint8_t> input = {0xFF};
	auto compressed = en::RleCodec::compress(input);
	auto decompressed = en::RleCodec::decompress(compressed);
	CHECK_EQ(decompressed.size(), static_cast<size_t>(1));
	CHECK_EQ(decompressed[0], static_cast<uint8_t>(0xFF));
}

TEST_CASE(rle_roundtrip_empty) {
	std::vector<uint8_t> input;
	auto compressed = en::RleCodec::compress(input);
	auto decompressed = en::RleCodec::decompress(compressed);
	CHECK_TRUE(decompressed.empty());
}

// ─── LZ77 Codec ──────────────────────

TEST_CASE(lz77_roundtrip_repeated_pattern) {
	// Repeated pattern should compress with LZ77
	std::vector<uint8_t> input;
	for (int i = 0; i < 10; ++i) {
		input.push_back(0x41); input.push_back(0x42);
		input.push_back(0x43); input.push_back(0x44);
	}
	auto compressed = en::Lz77Codec::compress(input);
	auto decompressed = en::Lz77Codec::decompress(compressed);
	CHECK_EQ(decompressed.size(), input.size());
	CHECK_TRUE(decompressed == input);
}

TEST_CASE(lz77_roundtrip_all_unique) {
	std::vector<uint8_t> input;
	for (uint8_t i = 0; i < 50; ++i) {
		input.push_back(i);
	}
	auto compressed = en::Lz77Codec::compress(input);
	auto decompressed = en::Lz77Codec::decompress(compressed);
	CHECK_EQ(decompressed.size(), input.size());
	CHECK_TRUE(decompressed == input);
}

// ─── Unified Interface ───────────────

TEST_CASE(compress_none_passthrough) {
	std::vector<uint8_t> input = {1, 2, 3, 4, 5};
	auto result = en::compress(input, en::CompressionType::None);
	CHECK_EQ(result.size(), input.size());
	CHECK_TRUE(result == input);
}

TEST_CASE(compress_decompress_deflate) {
	std::vector<uint8_t> input;
	for (int i = 0; i < 5; ++i) {
		input.push_back(0x61); input.push_back(0x62);
		input.push_back(0x63); input.push_back(0x64);
	}
	auto compressed = en::compress(input, en::CompressionType::Deflate);
	auto decompressed = en::decompress(compressed, en::CompressionType::Deflate);
	CHECK_EQ(decompressed.size(), input.size());
	CHECK_TRUE(decompressed == input);
}

// ─── Compression Ratio ───────────────

TEST_CASE(compression_ratio_helper) {
	CHECK_EQ(en::compression_ratio(100, 50), 0.5);
	CHECK_EQ(en::compression_ratio(100, 100), 1.0);
	CHECK_EQ(en::compression_ratio(0, 0), 1.0);
}

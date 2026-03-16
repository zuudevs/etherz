/**
 * @file compressed_socket.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Transparent per-connection compression wrapper
 * @version 3.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "compression.hpp"
#include "socket.hpp"
#include "internet_protocol.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  CompressedSocket<T>
// ═══════════════════════════════════════════════

/**
 * @brief Socket wrapper that compresses all data transparently
 * 
 * Wraps a Socket<T> and applies compression to all send()
 * calls and decompression to all recv() calls.
 * 
 * Wire format per frame:
 *   [4 bytes: original size (BE)] [4 bytes: compressed size (BE)] [compressed data]
 * 
 * Usage:
 *   CompressedSocket<Ip<4>> sock(CompressionType::Deflate);
 *   sock.create();
 *   sock.connect(addr);
 *   sock.send(large_data);  // Compressed on wire
 * 
 * @tparam T IP protocol type (Ip<4> or Ip<6>)
 */
template <typename T>
class CompressedSocket {
	static_assert(std::is_same_v<T, Ip<4>> || std::is_same_v<T, Ip<6>>,
		"Invalid IP version.");

public:
	using address_type = SocketAddress<T>;

	explicit CompressedSocket(CompressionType type = CompressionType::Deflate) noexcept
		: compression_type_(type) {}

	~CompressedSocket() noexcept { close(); }

	// Non-copyable
	CompressedSocket(const CompressedSocket&) = delete;
	CompressedSocket& operator=(const CompressedSocket&) = delete;

	// ─── Delegated Socket Operations ────

	core::Error create() noexcept { return socket_.create(); }
	core::Error bind(const address_type& addr) noexcept { return socket_.bind(addr); }
	core::Error connect(const address_type& addr) noexcept { return socket_.connect(addr); }
	core::Error listen(int backlog = 5) noexcept { return socket_.listen(backlog); }
	void close() noexcept { socket_.close(); }
	bool is_open() const noexcept { return socket_.is_open(); }

	// ─── Compressed Send ────────────────

	/**
	 * @brief Send data with compression
	 * @return Total bytes sent on wire (header + compressed data)
	 */
	int send(std::span<const uint8_t> data) noexcept {
		auto compressed = compress(data, compression_type_);

		// Build framed message
		uint32_t orig_size = static_cast<uint32_t>(data.size());
		uint32_t comp_size = static_cast<uint32_t>(compressed.size());

		std::vector<uint8_t> frame;
		frame.reserve(8 + compressed.size());

		// Original size (4 bytes BE)
		frame.push_back(static_cast<uint8_t>((orig_size >> 24) & 0xFF));
		frame.push_back(static_cast<uint8_t>((orig_size >> 16) & 0xFF));
		frame.push_back(static_cast<uint8_t>((orig_size >> 8) & 0xFF));
		frame.push_back(static_cast<uint8_t>(orig_size & 0xFF));

		// Compressed size (4 bytes BE)
		frame.push_back(static_cast<uint8_t>((comp_size >> 24) & 0xFF));
		frame.push_back(static_cast<uint8_t>((comp_size >> 16) & 0xFF));
		frame.push_back(static_cast<uint8_t>((comp_size >> 8) & 0xFF));
		frame.push_back(static_cast<uint8_t>(comp_size & 0xFF));

		frame.insert(frame.end(), compressed.begin(), compressed.end());

		bytes_raw_ += data.size();
		bytes_compressed_ += frame.size();

		return socket_.send(std::span<const uint8_t>(frame.data(), frame.size()));
	}

	// ─── Compressed Recv ────────────────

	/**
	 * @brief Receive and decompress data
	 * @return Number of decompressed bytes
	 */
	int recv(std::span<uint8_t> buffer) noexcept {
		// Read frame header (8 bytes)
		std::array<uint8_t, 8> header{};
		int hdr_recv = socket_.recv(header);
		if (hdr_recv < 8) return hdr_recv;

		uint32_t orig_size = (static_cast<uint32_t>(header[0]) << 24)
		                   | (static_cast<uint32_t>(header[1]) << 16)
		                   | (static_cast<uint32_t>(header[2]) << 8)
		                   | header[3];
		uint32_t comp_size = (static_cast<uint32_t>(header[4]) << 24)
		                   | (static_cast<uint32_t>(header[5]) << 16)
		                   | (static_cast<uint32_t>(header[6]) << 8)
		                   | header[7];

		// Read compressed data
		std::vector<uint8_t> compressed(comp_size);
		size_t total_read = 0;
		while (total_read < comp_size) {
			int n = socket_.recv(std::span<uint8_t>(
				compressed.data() + total_read, comp_size - total_read));
			if (n <= 0) break;
			total_read += static_cast<size_t>(n);
		}

		// Decompress
		auto decompressed = decompress(
			std::span<const uint8_t>(compressed.data(), total_read),
			compression_type_);

		size_t copy_size = std::min(decompressed.size(),
			static_cast<size_t>(buffer.size()));
		std::memcpy(buffer.data(), decompressed.data(), copy_size);

		return static_cast<int>(copy_size);
	}

	// ─── Stats ──────────────────────────

	size_t bytes_raw() const noexcept { return bytes_raw_; }
	size_t bytes_compressed() const noexcept { return bytes_compressed_; }

	double ratio() const noexcept {
		return compression_ratio(bytes_raw_, bytes_compressed_);
	}

	CompressionType compression_type() const noexcept { return compression_type_; }

	Socket<T>& underlying() noexcept { return socket_; }

private:
	Socket<T>       socket_;
	CompressionType compression_type_;
	size_t          bytes_raw_ = 0;
	size_t          bytes_compressed_ = 0;
};

} // namespace net
} // namespace etherz

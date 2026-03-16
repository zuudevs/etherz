/**
 * @file stream.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Ring-buffered ByteStream with backpressure
 * @version 3.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <cstring>
#include <algorithm>

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  ByteStream — Ring Buffer
// ═══════════════════════════════════════════════

/**
 * @brief Ring-buffered byte stream with backpressure
 * 
 * Provides a fixed-capacity circular buffer for streaming data
 * between a producer and consumer. Write operations block/fail
 * when the buffer is full (backpressure).
 * 
 * Usage:
 *   ByteStream stream(8192);
 *   stream.write(data);
 *   auto bytes = stream.read(buffer);
 */
class ByteStream {
public:
	/**
	 * @brief Create a ByteStream with the given capacity
	 * @param capacity Buffer size in bytes (default 8192)
	 */
	explicit ByteStream(size_t capacity = 8192) noexcept
		: buffer_(capacity), capacity_(capacity) {}

	// Non-copyable, movable
	ByteStream(const ByteStream&) = delete;
	ByteStream& operator=(const ByteStream&) = delete;
	ByteStream(ByteStream&&) noexcept = default;
	ByteStream& operator=(ByteStream&&) noexcept = default;

	// ─── Write (Producer) ───────────────

	/**
	 * @brief Write data to the stream
	 * @return Number of bytes actually written (may be less than input if full)
	 */
	size_t write(std::span<const uint8_t> data) noexcept {
		size_t avail = available_write();
		size_t to_write = std::min(data.size(), avail);
		if (to_write == 0) return 0;

		for (size_t i = 0; i < to_write; ++i) {
			buffer_[write_pos_ % capacity_] = data[i];
			++write_pos_;
		}

		total_written_ += to_write;
		return to_write;
	}

	/**
	 * @brief Write a single byte
	 */
	bool write_byte(uint8_t byte) noexcept {
		if (available_write() == 0) return false;
		buffer_[write_pos_ % capacity_] = byte;
		++write_pos_;
		++total_written_;
		return true;
	}

	// ─── Read (Consumer) ────────────────

	/**
	 * @brief Read data from the stream
	 * @return Number of bytes actually read
	 */
	size_t read(std::span<uint8_t> buffer) noexcept {
		size_t avail = available_read();
		size_t to_read = std::min(buffer.size(), avail);
		if (to_read == 0) return 0;

		for (size_t i = 0; i < to_read; ++i) {
			buffer[i] = buffer_[read_pos_ % capacity_];
			++read_pos_;
		}

		total_read_ += to_read;
		return to_read;
	}

	/**
	 * @brief Read a single byte
	 * @return -1 if empty, else the byte value
	 */
	int read_byte() noexcept {
		if (available_read() == 0) return -1;
		uint8_t byte = buffer_[read_pos_ % capacity_];
		++read_pos_;
		++total_read_;
		return byte;
	}

	/**
	 * @brief Peek at the next byte without consuming
	 */
	int peek() const noexcept {
		if (available_read() == 0) return -1;
		return buffer_[read_pos_ % capacity_];
	}

	/**
	 * @brief Skip bytes without reading them
	 */
	size_t skip(size_t count) noexcept {
		size_t avail = available_read();
		size_t to_skip = std::min(count, avail);
		read_pos_ += to_skip;
		total_read_ += to_skip;
		return to_skip;
	}

	// ─── Capacity ───────────────────────

	size_t available_read() const noexcept { return write_pos_ - read_pos_; }
	size_t available_write() const noexcept { return capacity_ - available_read(); }
	size_t capacity() const noexcept { return capacity_; }
	bool empty() const noexcept { return read_pos_ == write_pos_; }
	bool full() const noexcept { return available_write() == 0; }

	// ─── Stats ──────────────────────────

	size_t total_written() const noexcept { return total_written_; }
	size_t total_read() const noexcept { return total_read_; }

	/**
	 * @brief Reset the stream (clear all data)
	 */
	void reset() noexcept {
		read_pos_ = 0;
		write_pos_ = 0;
	}

private:
	std::vector<uint8_t> buffer_;
	size_t capacity_;
	size_t read_pos_ = 0;
	size_t write_pos_ = 0;
	size_t total_written_ = 0;
	size_t total_read_ = 0;
};

} // namespace net
} // namespace etherz

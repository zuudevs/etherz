/**
 * @file pipe.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief In-process bidirectional byte stream (like Unix pipe)
 * @version 3.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <span>
#include <utility>

#include "stream.hpp"

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Pipe End
// ═══════════════════════════════════════════════

/**
 * @brief One end of a bidirectional pipe
 * 
 * Writing to this end pushes data to the other end's read buffer.
 * Reading from this end pulls data that the other end wrote.
 */
class PipeEnd {
public:
	/**
	 * @brief Write data (will be readable from the other end)
	 */
	size_t write(std::span<const uint8_t> data) noexcept {
		if (!tx_) return 0;
		return tx_->write(data);
	}

	/**
	 * @brief Read data (written by the other end)
	 */
	size_t read(std::span<uint8_t> buffer) noexcept {
		if (!rx_) return 0;
		return rx_->read(buffer);
	}

	size_t available_read() const noexcept { return rx_ ? rx_->available_read() : 0; }
	size_t available_write() const noexcept { return tx_ ? tx_->available_write() : 0; }
	bool empty() const noexcept { return !rx_ || rx_->empty(); }

private:
	friend class Pipe;
	ByteStream* tx_ = nullptr;  // Write to this (other end reads)
	ByteStream* rx_ = nullptr;  // Read from this (other end writes)
};

// ═══════════════════════════════════════════════
//  Pipe — Bidirectional In-Process Stream
// ═══════════════════════════════════════════════

/**
 * @brief Bidirectional in-process byte pipe
 * 
 * Creates two connected endpoints. Data written to one end
 * can be read from the other, and vice versa.
 * 
 * Usage:
 *   Pipe pipe(4096);
 *   auto& [a, b] = pipe.ends();
 *   
 *   a.write(data);          // Push data to B
 *   b.read(buffer);         // Read what A wrote
 *   
 *   b.write(response);      // Push data to A
 *   a.read(resp_buffer);    // Read what B wrote
 */
class Pipe {
public:
	/**
	 * @brief Create a bidirectional pipe
	 * @param capacity Buffer capacity for each direction
	 */
	explicit Pipe(size_t capacity = 4096)
		: a_to_b_(capacity), b_to_a_(capacity)
	{
		end_a_.tx_ = &a_to_b_;
		end_a_.rx_ = &b_to_a_;

		end_b_.tx_ = &b_to_a_;
		end_b_.rx_ = &a_to_b_;
	}

	// Non-copyable
	Pipe(const Pipe&) = delete;
	Pipe& operator=(const Pipe&) = delete;

	/**
	 * @brief Get both pipe ends as a pair reference
	 */
	std::pair<PipeEnd&, PipeEnd&> ends() noexcept {
		return {end_a_, end_b_};
	}

	PipeEnd& end_a() noexcept { return end_a_; }
	PipeEnd& end_b() noexcept { return end_b_; }

	/**
	 * @brief Reset both directions
	 */
	void reset() noexcept {
		a_to_b_.reset();
		b_to_a_.reset();
	}

private:
	ByteStream a_to_b_;   // A writes → B reads
	ByteStream b_to_a_;   // B writes → A reads
	PipeEnd    end_a_;
	PipeEnd    end_b_;
};

} // namespace net
} // namespace etherz

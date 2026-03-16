#include "test_framework.hpp"
#include "net/stream.hpp"

namespace en = etherz::net;

// ─── ByteStream Construction (v3.0.0) ─

TEST_CASE(bytestream_construction) {
	en::ByteStream stream(256);
	CHECK_EQ(stream.capacity(), static_cast<size_t>(256));
	CHECK_TRUE(stream.empty());
	CHECK_FALSE(stream.full());
	CHECK_EQ(stream.available_read(), static_cast<size_t>(0));
	CHECK_EQ(stream.available_write(), static_cast<size_t>(256));
}

// ─── Write + Read ─────────────────────

TEST_CASE(bytestream_write_read) {
	en::ByteStream stream(64);
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	auto written = stream.write(data);
	CHECK_EQ(written, static_cast<size_t>(5));
	CHECK_EQ(stream.available_read(), static_cast<size_t>(5));

	std::vector<uint8_t> buffer(5);
	auto read = stream.read(buffer);
	CHECK_EQ(read, static_cast<size_t>(5));
	CHECK_EQ(buffer[0], static_cast<uint8_t>(1));
	CHECK_EQ(buffer[4], static_cast<uint8_t>(5));
	CHECK_TRUE(stream.empty());
}

// ─── write_byte / read_byte ───────────

TEST_CASE(bytestream_byte_ops) {
	en::ByteStream stream(16);
	CHECK_TRUE(stream.write_byte(0xAA));
	CHECK_TRUE(stream.write_byte(0xBB));

	CHECK_EQ(stream.read_byte(), 0xAA);
	CHECK_EQ(stream.read_byte(), 0xBB);
	CHECK_EQ(stream.read_byte(), -1); // empty
}

// ─── Peek ─────────────────────────────

TEST_CASE(bytestream_peek) {
	en::ByteStream stream(16);
	CHECK_EQ(stream.peek(), -1); // empty

	stream.write_byte(0x42);
	CHECK_EQ(stream.peek(), 0x42);
	// Peek should not consume
	CHECK_EQ(stream.available_read(), static_cast<size_t>(1));
}

// ─── Skip ─────────────────────────────

TEST_CASE(bytestream_skip) {
	en::ByteStream stream(32);
	std::vector<uint8_t> data = {10, 20, 30, 40, 50};
	stream.write(data);

	auto skipped = stream.skip(3);
	CHECK_EQ(skipped, static_cast<size_t>(3));
	CHECK_EQ(stream.available_read(), static_cast<size_t>(2));
	CHECK_EQ(stream.read_byte(), 40);
}

// ─── Backpressure ─────────────────────

TEST_CASE(bytestream_backpressure) {
	en::ByteStream stream(4); // tiny capacity
	std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6};
	auto written = stream.write(data);
	CHECK_EQ(written, static_cast<size_t>(4)); // only 4 fit
	CHECK_TRUE(stream.full());
	CHECK_EQ(stream.available_write(), static_cast<size_t>(0));

	// write_byte should fail when full
	CHECK_FALSE(stream.write_byte(0xFF));
}

// ─── Stats ────────────────────────────

TEST_CASE(bytestream_stats) {
	en::ByteStream stream(64);
	std::vector<uint8_t> data = {1, 2, 3};
	stream.write(data);
	CHECK_EQ(stream.total_written(), static_cast<size_t>(3));

	std::vector<uint8_t> buf(2);
	stream.read(buf);
	CHECK_EQ(stream.total_read(), static_cast<size_t>(2));
}

// ─── Reset ────────────────────────────

TEST_CASE(bytestream_reset) {
	en::ByteStream stream(32);
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	stream.write(data);
	CHECK_FALSE(stream.empty());

	stream.reset();
	CHECK_TRUE(stream.empty());
	CHECK_EQ(stream.available_read(), static_cast<size_t>(0));
	CHECK_EQ(stream.available_write(), static_cast<size_t>(32));
}

// ─── Wraparound ───────────────────────

TEST_CASE(bytestream_wraparound) {
	en::ByteStream stream(4);
	// Fill and drain multiple times to test ring buffer wraparound
	for (int round = 0; round < 3; ++round) {
		std::vector<uint8_t> data = {10, 20, 30, 40};
		auto w = stream.write(data);
		CHECK_EQ(w, static_cast<size_t>(4));

		std::vector<uint8_t> buf(4);
		auto r = stream.read(buf);
		CHECK_EQ(r, static_cast<size_t>(4));
		CHECK_EQ(buf[0], static_cast<uint8_t>(10));
		CHECK_EQ(buf[3], static_cast<uint8_t>(40));
	}
}

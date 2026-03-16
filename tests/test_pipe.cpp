#include "test_framework.hpp"
#include "net/pipe.hpp"

namespace en = etherz::net;

// ─── Pipe Construction (v3.0.0) ──────

TEST_CASE(pipe_construction) {
	en::Pipe pipe(128);
	auto& a = pipe.end_a();
	auto& b = pipe.end_b();
	CHECK_TRUE(a.empty());
	CHECK_TRUE(b.empty());
}

// ─── A → B Data Transfer ─────────────

TEST_CASE(pipe_a_to_b) {
	en::Pipe pipe(64);
	auto& a = pipe.end_a();
	auto& b = pipe.end_b();

	std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F}; // "Hello"
	auto written = a.write(data);
	CHECK_EQ(written, static_cast<size_t>(5));

	// B should be able to read what A wrote
	CHECK_EQ(b.available_read(), static_cast<size_t>(5));
	std::vector<uint8_t> buf(5);
	auto read = b.read(buf);
	CHECK_EQ(read, static_cast<size_t>(5));
	CHECK_EQ(buf[0], static_cast<uint8_t>(0x48));
	CHECK_EQ(buf[4], static_cast<uint8_t>(0x6F));
}

// ─── B → A Data Transfer ─────────────

TEST_CASE(pipe_b_to_a) {
	en::Pipe pipe(64);
	auto& a = pipe.end_a();
	auto& b = pipe.end_b();

	std::vector<uint8_t> response = {0x4F, 0x4B}; // "OK"
	b.write(response);

	CHECK_EQ(a.available_read(), static_cast<size_t>(2));
	std::vector<uint8_t> buf(2);
	a.read(buf);
	CHECK_EQ(buf[0], static_cast<uint8_t>(0x4F));
	CHECK_EQ(buf[1], static_cast<uint8_t>(0x4B));
}

// ─── Bidirectional Transfer ──────────

TEST_CASE(pipe_bidirectional) {
	en::Pipe pipe(64);
	auto& a = pipe.end_a();
	auto& b = pipe.end_b();

	// A → B
	std::vector<uint8_t> msg1 = {1, 2, 3};
	a.write(msg1);

	// B → A
	std::vector<uint8_t> msg2 = {4, 5, 6};
	b.write(msg2);

	// Each side reads what the other wrote
	std::vector<uint8_t> buf(3);
	b.read(buf);
	CHECK_EQ(buf[0], static_cast<uint8_t>(1));

	a.read(buf);
	CHECK_EQ(buf[0], static_cast<uint8_t>(4));
}

// ─── Available Write ─────────────────

TEST_CASE(pipe_available_write) {
	en::Pipe pipe(8);
	auto& a = pipe.end_a();
	CHECK_EQ(a.available_write(), static_cast<size_t>(8));

	std::vector<uint8_t> data = {1, 2, 3};
	a.write(data);
	CHECK_EQ(a.available_write(), static_cast<size_t>(5));
}

// ─── Reset ──────────────────────────

TEST_CASE(pipe_reset) {
	en::Pipe pipe(32);
	auto& a = pipe.end_a();
	auto& b = pipe.end_b();

	std::vector<uint8_t> data = {10, 20, 30};
	a.write(data);
	b.write(data);

	CHECK_FALSE(b.empty()); // B has data from A
	CHECK_FALSE(a.empty()); // A has data from B

	pipe.reset();

	CHECK_TRUE(a.empty());
	CHECK_TRUE(b.empty());
}

// ─── Empty Pipe Read ────────────────

TEST_CASE(pipe_empty_read) {
	en::Pipe pipe(16);
	auto& b = pipe.end_b();

	std::vector<uint8_t> buf(8);
	auto read = b.read(buf);
	CHECK_EQ(read, static_cast<size_t>(0));
}

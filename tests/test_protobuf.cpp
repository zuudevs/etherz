#include "test_framework.hpp"
#include "protocol/protobuf.hpp"

namespace ep = etherz::protocol;

// ─── ProtoMessage Varint ───────────────

TEST_CASE(proto_set_get_varint) {
	ep::ProtoMessage msg;
	msg.set_varint(1, 42);
	CHECK_EQ(msg.get_varint(1), static_cast<uint64_t>(42));
	CHECK_EQ(msg.get_varint(99), static_cast<uint64_t>(0)); // default
}

TEST_CASE(proto_set_get_string) {
	ep::ProtoMessage msg;
	msg.set_string(2, "hello");
	CHECK_EQ(msg.get_string(2), std::string("hello"));
	CHECK_EQ(msg.get_string(99), std::string("")); // default
}

TEST_CASE(proto_has_field) {
	ep::ProtoMessage msg;
	CHECK_FALSE(msg.has_field(1));
	msg.set_varint(1, 10);
	CHECK_TRUE(msg.has_field(1));
}

TEST_CASE(proto_serialize_deserialize_varint) {
	ep::ProtoMessage original;
	original.set_varint(1, 100);
	original.set_varint(2, 999999);

	auto wire = original.serialize();
	auto decoded = ep::ProtoMessage::deserialize(wire);

	CHECK_EQ(decoded.get_varint(1), static_cast<uint64_t>(100));
	CHECK_EQ(decoded.get_varint(2), static_cast<uint64_t>(999999));
}

TEST_CASE(proto_serialize_deserialize_string) {
	ep::ProtoMessage original;
	original.set_string(1, "hello world");

	auto wire = original.serialize();
	auto decoded = ep::ProtoMessage::deserialize(wire);

	CHECK_EQ(decoded.get_string(1), std::string("hello world"));
}

TEST_CASE(proto_multiple_fields) {
	ep::ProtoMessage msg;
	msg.set_varint(1, 42);
	msg.set_string(2, "test");
	msg.set_varint(3, 0);

	auto wire = msg.serialize();
	auto decoded = ep::ProtoMessage::deserialize(wire);

	CHECK_EQ(decoded.get_varint(1), static_cast<uint64_t>(42));
	CHECK_EQ(decoded.get_string(2), std::string("test"));
	CHECK_EQ(decoded.get_varint(3), static_cast<uint64_t>(0));
}

TEST_CASE(proto_fields_count) {
	ep::ProtoMessage msg;
	msg.set_varint(1, 10);
	msg.set_string(2, "hi");
	CHECK_EQ(msg.fields().size(), static_cast<size_t>(2));
}

// ─── gRPC Framing ─────────────────────

TEST_CASE(grpc_encode_decode_uncompressed) {
	std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
	auto encoded = ep::grpc_encode_message(payload, false);

	CHECK_EQ(encoded[0], static_cast<uint8_t>(0)); // not compressed
	CHECK_EQ(encoded.size(), static_cast<size_t>(5 + 4));

	bool compressed = true;
	auto decoded = ep::grpc_decode_message(encoded, compressed);
	CHECK_FALSE(compressed);
	CHECK_EQ(decoded.size(), static_cast<size_t>(4));
	CHECK_EQ(decoded[0], static_cast<uint8_t>(0x01));
	CHECK_EQ(decoded[3], static_cast<uint8_t>(0x04));
}

TEST_CASE(grpc_encode_decode_compressed) {
	std::vector<uint8_t> payload = {0xAA, 0xBB};
	auto encoded = ep::grpc_encode_message(payload, true);

	CHECK_EQ(encoded[0], static_cast<uint8_t>(1)); // compressed

	bool compressed = false;
	auto decoded = ep::grpc_decode_message(encoded, compressed);
	CHECK_TRUE(compressed);
	CHECK_EQ(decoded.size(), static_cast<size_t>(2));
}

TEST_CASE(grpc_decode_too_short) {
	std::vector<uint8_t> data = {0, 0, 0};
	bool compressed = false;
	auto decoded = ep::grpc_decode_message(data, compressed);
	CHECK_TRUE(decoded.empty());
}

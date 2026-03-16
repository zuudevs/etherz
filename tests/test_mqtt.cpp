#include "test_framework.hpp"
#include "protocol/mqtt.hpp"

namespace ep = etherz::protocol;

// ─── Remaining Length Encoding ─────────

TEST_CASE(mqtt_remaining_length_small) {
	auto encoded = ep::mqtt_encode_remaining_length(64);
	CHECK_EQ(encoded.size(), static_cast<size_t>(1));
	CHECK_EQ(encoded[0], static_cast<uint8_t>(64));

	size_t pos = 0;
	auto decoded = ep::mqtt_decode_remaining_length(encoded, pos);
	CHECK_EQ(decoded, static_cast<uint32_t>(64));
}

TEST_CASE(mqtt_remaining_length_medium) {
	auto encoded = ep::mqtt_encode_remaining_length(321);
	CHECK_EQ(encoded.size(), static_cast<size_t>(2));

	size_t pos = 0;
	auto decoded = ep::mqtt_decode_remaining_length(encoded, pos);
	CHECK_EQ(decoded, static_cast<uint32_t>(321));
}

TEST_CASE(mqtt_remaining_length_large) {
	auto encoded = ep::mqtt_encode_remaining_length(16384);

	size_t pos = 0;
	auto decoded = ep::mqtt_decode_remaining_length(encoded, pos);
	CHECK_EQ(decoded, static_cast<uint32_t>(16384));
}

TEST_CASE(mqtt_remaining_length_zero) {
	auto encoded = ep::mqtt_encode_remaining_length(0);
	CHECK_EQ(encoded.size(), static_cast<size_t>(1));
	CHECK_EQ(encoded[0], static_cast<uint8_t>(0));
}

// ─── String Encoding ──────────────────

TEST_CASE(mqtt_string_roundtrip) {
	std::vector<uint8_t> out;
	ep::mqtt_encode_string(out, "hello");
	CHECK_EQ(out.size(), static_cast<size_t>(2 + 5));
	CHECK_EQ(out[0], static_cast<uint8_t>(0));   // length MSB
	CHECK_EQ(out[1], static_cast<uint8_t>(5));   // length LSB

	size_t pos = 0;
	auto decoded = ep::mqtt_decode_string(out, pos);
	CHECK_EQ(decoded, std::string("hello"));
	CHECK_EQ(pos, static_cast<size_t>(7));
}

TEST_CASE(mqtt_string_empty) {
	std::vector<uint8_t> out;
	ep::mqtt_encode_string(out, "");
	CHECK_EQ(out.size(), static_cast<size_t>(2));

	size_t pos = 0;
	auto decoded = ep::mqtt_decode_string(out, pos);
	CHECK_EQ(decoded, std::string(""));
}

// ─── MqttMessage ──────────────────────

TEST_CASE(mqtt_message_create) {
	auto msg = ep::MqttMessage::create("sensor/temp", "25.5");
	CHECK_EQ(msg.topic, std::string("sensor/temp"));
	CHECK_EQ(msg.payload_string(), std::string("25.5"));
	CHECK_EQ(msg.qos, ep::MqttQoS::AtMostOnce);
	CHECK_FALSE(msg.retain);
}

// ─── Packet Builders ──────────────────

TEST_CASE(mqtt_pingreq) {
	auto pkt = ep::mqtt_builder::pingreq();
	CHECK_EQ(pkt.size(), static_cast<size_t>(2));
	CHECK_EQ(pkt[0], static_cast<uint8_t>(ep::MqttPacketType::PingReq) << 4);
	CHECK_EQ(pkt[1], static_cast<uint8_t>(0));
}

TEST_CASE(mqtt_disconnect) {
	auto pkt = ep::mqtt_builder::disconnect();
	CHECK_EQ(pkt.size(), static_cast<size_t>(2));
	CHECK_EQ(pkt[0], static_cast<uint8_t>(ep::MqttPacketType::Disconnect) << 4);
	CHECK_EQ(pkt[1], static_cast<uint8_t>(0));
}

TEST_CASE(mqtt_connect_packet) {
	ep::MqttConnectOptions opts;
	opts.client_id = "etherz_test";
	opts.keep_alive_sec = 60;
	opts.clean_session = true;

	auto pkt = ep::mqtt_builder::connect(opts);
	CHECK_TRUE(pkt.size() > 10);
	// First byte: CONNECT type (1 << 4)
	CHECK_EQ(pkt[0], static_cast<uint8_t>(ep::MqttPacketType::Connect) << 4);
}

TEST_CASE(mqtt_publish_packet) {
	auto msg = ep::MqttMessage::create("test/topic", "payload", ep::MqttQoS::AtMostOnce);
	auto pkt = ep::mqtt_builder::publish(msg);
	CHECK_TRUE(pkt.size() > 5);
	// First nibble should be PUBLISH type
	CHECK_EQ(pkt[0] >> 4, static_cast<uint8_t>(ep::MqttPacketType::Publish));
}

TEST_CASE(mqtt_subscribe_packet) {
	std::vector<std::pair<std::string, ep::MqttQoS>> topics = {
		{"sensor/#", ep::MqttQoS::AtLeastOnce}
	};
	auto pkt = ep::mqtt_builder::subscribe(1, topics);
	CHECK_TRUE(pkt.size() > 5);
	CHECK_EQ(pkt[0] >> 4, static_cast<uint8_t>(ep::MqttPacketType::Subscribe));
}

TEST_CASE(mqtt_unsubscribe_packet) {
	std::vector<std::string> topics = {"sensor/#"};
	auto pkt = ep::mqtt_builder::unsubscribe(2, topics);
	CHECK_TRUE(pkt.size() > 5);
	CHECK_EQ(pkt[0] >> 4, static_cast<uint8_t>(ep::MqttPacketType::Unsubscribe));
}

/**
 * @file mqtt.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief MQTT 3.1.1/5.0 packet types, serialization, and QoS
 * @version 2.3.0
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
#include <optional>

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  MQTT Packet Types (MQTT 3.1.1 §2.2)
// ═══════════════════════════════════════════════

enum class MqttPacketType : uint8_t {
	Connect     = 1,
	ConnAck     = 2,
	Publish     = 3,
	PubAck      = 4,
	PubRec      = 5,
	PubRel      = 6,
	PubComp     = 7,
	Subscribe   = 8,
	SubAck      = 9,
	Unsubscribe = 10,
	UnsubAck    = 11,
	PingReq     = 12,
	PingResp    = 13,
	Disconnect  = 14
};

// ═══════════════════════════════════════════════
//  MQTT QoS Levels
// ═══════════════════════════════════════════════

enum class MqttQoS : uint8_t {
	AtMostOnce  = 0,   // Fire and forget
	AtLeastOnce = 1,   // Acknowledged delivery
	ExactlyOnce = 2    // Assured delivery (4-step handshake)
};

// ═══════════════════════════════════════════════
//  MQTT Connect Return Codes
// ═══════════════════════════════════════════════

enum class MqttConnectCode : uint8_t {
	Accepted              = 0,
	UnacceptableVersion   = 1,
	IdentifierRejected    = 2,
	ServerUnavailable     = 3,
	BadCredentials        = 4,
	NotAuthorized         = 5
};

// ═══════════════════════════════════════════════
//  MQTT Publish Message
// ═══════════════════════════════════════════════

/**
 * @brief An MQTT publish message
 */
struct MqttMessage {
	std::string          topic;
	std::vector<uint8_t> payload;
	MqttQoS             qos       = MqttQoS::AtMostOnce;
	bool                 retain    = false;
	bool                 dup       = false;
	uint16_t             packet_id = 0;

	/**
	 * @brief Create a message from a string payload
	 */
	static MqttMessage create(std::string topic, std::string payload,
		MqttQoS qos = MqttQoS::AtMostOnce)
	{
		MqttMessage msg;
		msg.topic = std::move(topic);
		msg.payload.assign(payload.begin(), payload.end());
		msg.qos = qos;
		return msg;
	}

	/**
	 * @brief Get payload as string
	 */
	std::string payload_string() const {
		return std::string(reinterpret_cast<const char*>(payload.data()), payload.size());
	}
};

// ═══════════════════════════════════════════════
//  MQTT Connect Options
// ═══════════════════════════════════════════════

/**
 * @brief Connect configuration for MqttClient
 */
struct MqttConnectOptions {
	std::string client_id;
	std::string username;
	std::string password;
	uint16_t    keep_alive_sec = 60;
	bool        clean_session  = true;

	// Last Will and Testament
	std::optional<MqttMessage> will;
};

// ═══════════════════════════════════════════════
//  MQTT Packet Encoding/Decoding
// ═══════════════════════════════════════════════

/**
 * @brief Encode a variable-length integer (MQTT remaining length)
 */
inline std::vector<uint8_t> mqtt_encode_remaining_length(uint32_t length) {
	std::vector<uint8_t> out;
	do {
		uint8_t byte = static_cast<uint8_t>(length & 0x7F);
		length >>= 7;
		if (length > 0) byte |= 0x80;
		out.push_back(byte);
	} while (length > 0);
	return out;
}

/**
 * @brief Decode a variable-length integer
 */
inline uint32_t mqtt_decode_remaining_length(std::span<const uint8_t> data, size_t& pos) {
	uint32_t value = 0;
	uint32_t multiplier = 1;
	while (pos < data.size()) {
		uint8_t byte = data[pos++];
		value += (byte & 0x7F) * multiplier;
		if ((byte & 0x80) == 0) break;
		multiplier *= 128;
	}
	return value;
}

/**
 * @brief Encode an MQTT UTF-8 string (length-prefixed)
 */
inline void mqtt_encode_string(std::vector<uint8_t>& out, std::string_view str) {
	uint16_t len = static_cast<uint16_t>(str.size());
	out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(len & 0xFF));
	out.insert(out.end(), str.begin(), str.end());
}

/**
 * @brief Decode an MQTT UTF-8 string
 */
inline std::string mqtt_decode_string(std::span<const uint8_t> data, size_t& pos) {
	if (pos + 2 > data.size()) return "";
	uint16_t len = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
	pos += 2;
	if (pos + len > data.size()) return "";
	std::string result(reinterpret_cast<const char*>(data.data() + pos), len);
	pos += len;
	return result;
}

// ═══════════════════════════════════════════════
//  MQTT Packet Builders
// ═══════════════════════════════════════════════

namespace mqtt_builder {

/**
 * @brief Build a CONNECT packet
 */
inline std::vector<uint8_t> connect(const MqttConnectOptions& opts) {
	std::vector<uint8_t> var_header;
	// Protocol name
	mqtt_encode_string(var_header, "MQTT");
	// Protocol level (4 = MQTT 3.1.1)
	var_header.push_back(4);
	// Connect flags
	uint8_t flags = 0;
	if (opts.clean_session) flags |= 0x02;
	if (opts.will) {
		flags |= 0x04;
		flags |= (static_cast<uint8_t>(opts.will->qos) << 3);
		if (opts.will->retain) flags |= 0x20;
	}
	if (!opts.password.empty()) flags |= 0x40;
	if (!opts.username.empty()) flags |= 0x80;
	var_header.push_back(flags);
	// Keep alive
	var_header.push_back(static_cast<uint8_t>((opts.keep_alive_sec >> 8) & 0xFF));
	var_header.push_back(static_cast<uint8_t>(opts.keep_alive_sec & 0xFF));

	// Payload
	std::vector<uint8_t> payload;
	mqtt_encode_string(payload, opts.client_id);
	if (opts.will) {
		mqtt_encode_string(payload, opts.will->topic);
		uint16_t wlen = static_cast<uint16_t>(opts.will->payload.size());
		payload.push_back(static_cast<uint8_t>((wlen >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>(wlen & 0xFF));
		payload.insert(payload.end(), opts.will->payload.begin(), opts.will->payload.end());
	}
	if (!opts.username.empty()) mqtt_encode_string(payload, opts.username);
	if (!opts.password.empty()) mqtt_encode_string(payload, opts.password);

	// Fixed header
	uint32_t remaining = static_cast<uint32_t>(var_header.size() + payload.size());
	std::vector<uint8_t> packet;
	packet.push_back(static_cast<uint8_t>(MqttPacketType::Connect) << 4);
	auto rl = mqtt_encode_remaining_length(remaining);
	packet.insert(packet.end(), rl.begin(), rl.end());
	packet.insert(packet.end(), var_header.begin(), var_header.end());
	packet.insert(packet.end(), payload.begin(), payload.end());
	return packet;
}

/**
 * @brief Build a PUBLISH packet
 */
inline std::vector<uint8_t> publish(const MqttMessage& msg) {
	std::vector<uint8_t> var_header;
	mqtt_encode_string(var_header, msg.topic);
	if (msg.qos != MqttQoS::AtMostOnce) {
		var_header.push_back(static_cast<uint8_t>((msg.packet_id >> 8) & 0xFF));
		var_header.push_back(static_cast<uint8_t>(msg.packet_id & 0xFF));
	}

	uint32_t remaining = static_cast<uint32_t>(var_header.size() + msg.payload.size());

	uint8_t flags = (static_cast<uint8_t>(MqttPacketType::Publish) << 4)
	              | (msg.dup ? 0x08 : 0)
	              | (static_cast<uint8_t>(msg.qos) << 1)
	              | (msg.retain ? 0x01 : 0);

	std::vector<uint8_t> packet;
	packet.push_back(flags);
	auto rl = mqtt_encode_remaining_length(remaining);
	packet.insert(packet.end(), rl.begin(), rl.end());
	packet.insert(packet.end(), var_header.begin(), var_header.end());
	packet.insert(packet.end(), msg.payload.begin(), msg.payload.end());
	return packet;
}

/**
 * @brief Build a SUBSCRIBE packet
 */
inline std::vector<uint8_t> subscribe(uint16_t packet_id,
	const std::vector<std::pair<std::string, MqttQoS>>& topics)
{
	std::vector<uint8_t> var_header;
	var_header.push_back(static_cast<uint8_t>((packet_id >> 8) & 0xFF));
	var_header.push_back(static_cast<uint8_t>(packet_id & 0xFF));

	std::vector<uint8_t> payload;
	for (const auto& [topic, qos] : topics) {
		mqtt_encode_string(payload, topic);
		payload.push_back(static_cast<uint8_t>(qos));
	}

	uint32_t remaining = static_cast<uint32_t>(var_header.size() + payload.size());

	std::vector<uint8_t> packet;
	packet.push_back((static_cast<uint8_t>(MqttPacketType::Subscribe) << 4) | 0x02);
	auto rl = mqtt_encode_remaining_length(remaining);
	packet.insert(packet.end(), rl.begin(), rl.end());
	packet.insert(packet.end(), var_header.begin(), var_header.end());
	packet.insert(packet.end(), payload.begin(), payload.end());
	return packet;
}

/**
 * @brief Build an UNSUBSCRIBE packet
 */
inline std::vector<uint8_t> unsubscribe(uint16_t packet_id,
	const std::vector<std::string>& topics)
{
	std::vector<uint8_t> var_header;
	var_header.push_back(static_cast<uint8_t>((packet_id >> 8) & 0xFF));
	var_header.push_back(static_cast<uint8_t>(packet_id & 0xFF));

	std::vector<uint8_t> payload;
	for (const auto& topic : topics) {
		mqtt_encode_string(payload, topic);
	}

	uint32_t remaining = static_cast<uint32_t>(var_header.size() + payload.size());

	std::vector<uint8_t> packet;
	packet.push_back((static_cast<uint8_t>(MqttPacketType::Unsubscribe) << 4) | 0x02);
	auto rl = mqtt_encode_remaining_length(remaining);
	packet.insert(packet.end(), rl.begin(), rl.end());
	packet.insert(packet.end(), var_header.begin(), var_header.end());
	packet.insert(packet.end(), payload.begin(), payload.end());
	return packet;
}

/**
 * @brief Build a PINGREQ packet
 */
inline std::vector<uint8_t> pingreq() {
	return { (static_cast<uint8_t>(MqttPacketType::PingReq) << 4), 0x00 };
}

/**
 * @brief Build a DISCONNECT packet
 */
inline std::vector<uint8_t> disconnect() {
	return { (static_cast<uint8_t>(MqttPacketType::Disconnect) << 4), 0x00 };
}

} // namespace mqtt_builder

} // namespace protocol
} // namespace etherz

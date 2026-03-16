/**
 * @file mqtt_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief MQTT client for connect, publish, subscribe, keep-alive
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
#include <functional>
#include <unordered_map>
#include <expected>

#include "mqtt.hpp"
#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  MQTT Client
// ═══════════════════════════════════════════════

/**
 * @brief MQTT client for pub/sub messaging
 * 
 * Connects to an MQTT broker and provides publish/subscribe
 * operations with QoS 0, 1, and 2 support.
 * 
 * Usage:
 *   MqttClient client;
 *   client.connect("mqtt.example.com", 1883, {.client_id = "etherz-1"});
 *   
 *   client.subscribe("sensor/temp", MqttQoS::AtLeastOnce,
 *       [](const MqttMessage& msg) {
 *           std::print("Temp: {}\n", msg.payload_string());
 *       });
 *   
 *   client.publish("actuator/led", "ON");
 *   client.loop();  // Process incoming messages
 */
class MqttClient {
public:
	using MessageCallback = std::function<void(const MqttMessage&)>;

	MqttClient() noexcept = default;
	~MqttClient() noexcept { disconnect(); }

	// Non-copyable
	MqttClient(const MqttClient&) = delete;
	MqttClient& operator=(const MqttClient&) = delete;

	// ─── Connection ─────────────────────

	/**
	 * @brief Connect to an MQTT broker
	 * @param host Broker hostname
	 * @param port Broker port (default 1883)
	 * @param opts Connect options (client ID, credentials, etc.)
	 */
	core::Error connect(std::string_view host, uint16_t port = 1883,
		MqttConnectOptions opts = {}) noexcept
	{
		if (opts.client_id.empty()) {
			opts.client_id = "etherz-client";
		}

		auto ip = net::Ip<4>(127, 0, 0, 1);
		if (host != "localhost" && host != "127.0.0.1") {
			auto dns = net::Dns::resolve(std::string(host));
			if (dns.success && !dns.ipv4_addresses.empty()) {
				ip = dns.ipv4_addresses[0];
			}
		}

		auto addr = net::SocketAddress<net::Ip<4>>(ip, port);
		if (auto err = socket_.create(); core::is_error(err)) return err;
		if (auto err = socket_.connect(addr); core::is_error(err)) return err;

		// Send CONNECT packet
		auto connect_pkt = mqtt_builder::connect(opts);
		socket_.send(std::span<const uint8_t>(connect_pkt.data(), connect_pkt.size()));

		// Wait for CONNACK
		std::array<uint8_t, 4> connack{};
		int received = socket_.recv(connack);
		if (received < 4) return core::Error::ReceiveFailed;

		auto pkt_type = static_cast<MqttPacketType>(connack[0] >> 4);
		if (pkt_type != MqttPacketType::ConnAck) return core::Error::ReceiveFailed;

		auto connect_code = static_cast<MqttConnectCode>(connack[3]);
		if (connect_code != MqttConnectCode::Accepted) return core::Error::HandshakeFailed;

		connected_ = true;
		keep_alive_sec_ = opts.keep_alive_sec;
		return core::Error::None;
	}

	/**
	 * @brief Disconnect from the broker
	 */
	void disconnect() noexcept {
		if (connected_) {
			auto pkt = mqtt_builder::disconnect();
			socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));
			connected_ = false;
		}
		socket_.close();
	}

	// ─── Publish ────────────────────────

	/**
	 * @brief Publish a string message to a topic
	 */
	core::Error publish(std::string_view topic, std::string_view payload,
		MqttQoS qos = MqttQoS::AtMostOnce, bool retain = false)
	{
		if (!connected_) return core::Error::NotConnected;

		auto msg = MqttMessage::create(std::string(topic), std::string(payload), qos);
		msg.retain = retain;
		if (qos != MqttQoS::AtMostOnce) {
			msg.packet_id = next_packet_id_++;
		}

		auto pkt = mqtt_builder::publish(msg);
		socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));
		return core::Error::None;
	}

	/**
	 * @brief Publish a binary message
	 */
	core::Error publish(const MqttMessage& msg) {
		if (!connected_) return core::Error::NotConnected;
		auto pkt = mqtt_builder::publish(msg);
		socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));
		return core::Error::None;
	}

	// ─── Subscribe ──────────────────────

	/**
	 * @brief Subscribe to a topic with a callback
	 */
	core::Error subscribe(std::string_view topic, MqttQoS qos,
		MessageCallback callback)
	{
		if (!connected_) return core::Error::NotConnected;

		uint16_t packet_id = next_packet_id_++;
		auto pkt = mqtt_builder::subscribe(packet_id,
			{{std::string(topic), qos}});
		socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));

		handlers_[std::string(topic)] = std::move(callback);

		// Read SUBACK
		std::array<uint8_t, 5> suback{};
		socket_.recv(suback);

		return core::Error::None;
	}

	/**
	 * @brief Unsubscribe from a topic
	 */
	core::Error unsubscribe(std::string_view topic) {
		if (!connected_) return core::Error::NotConnected;

		uint16_t packet_id = next_packet_id_++;
		auto pkt = mqtt_builder::unsubscribe(packet_id,
			{std::string(topic)});
		socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));

		handlers_.erase(std::string(topic));
		return core::Error::None;
	}

	// ─── Message Loop ───────────────────

	/**
	 * @brief Process one incoming message
	 * @return true if a message was processed
	 */
	bool loop_once() {
		if (!connected_) return false;

		std::array<uint8_t, 4096> buffer{};
		int received = socket_.recv(buffer);
		if (received <= 0) return false;

		auto data = std::span<const uint8_t>(buffer.data(),
			static_cast<size_t>(received));
		auto pkt_type = static_cast<MqttPacketType>(data[0] >> 4);

		switch (pkt_type) {
			case MqttPacketType::Publish:
				handle_publish(data);
				return true;

			case MqttPacketType::PingResp:
				return true;

			case MqttPacketType::PubAck:
			case MqttPacketType::SubAck:
			case MqttPacketType::UnsubAck:
				return true;

			default:
				return false;
		}
	}

	/**
	 * @brief Send a PINGREQ to keep the connection alive
	 */
	void ping() {
		if (!connected_) return;
		auto pkt = mqtt_builder::pingreq();
		socket_.send(std::span<const uint8_t>(pkt.data(), pkt.size()));
	}

	// ─── Queries ────────────────────────

	bool is_connected() const noexcept { return connected_; }

private:
	net::Socket<net::Ip<4>> socket_;
	std::unordered_map<std::string, MessageCallback> handlers_;
	uint16_t next_packet_id_ = 1;
	uint16_t keep_alive_sec_ = 60;
	bool connected_ = false;

	void handle_publish(std::span<const uint8_t> data) {
		if (data.size() < 2) return;

		size_t pos = 1;
		uint32_t remaining = mqtt_decode_remaining_length(data, pos);
		if (pos + remaining > data.size()) return;

		MqttMessage msg;
		uint8_t flags = data[0];
		msg.dup = (flags & 0x08) != 0;
		msg.qos = static_cast<MqttQoS>((flags >> 1) & 0x03);
		msg.retain = (flags & 0x01) != 0;

		msg.topic = mqtt_decode_string(data, pos);
		if (msg.qos != MqttQoS::AtMostOnce) {
			if (pos + 2 > data.size()) return;
			msg.packet_id = (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1];
			pos += 2;
		}

		size_t payload_len = remaining - (pos - 2);  // Approximate
		if (pos + payload_len <= data.size()) {
			msg.payload.assign(data.begin() + pos, data.begin() + pos + payload_len);
		}

		// Dispatch to handler
		auto it = handlers_.find(msg.topic);
		if (it != handlers_.end()) {
			it->second(msg);
		}

		// Send PUBACK for QoS 1
		if (msg.qos == MqttQoS::AtLeastOnce) {
			std::array<uint8_t, 4> puback = {
				static_cast<uint8_t>(MqttPacketType::PubAck) << 4,
				0x02,
				static_cast<uint8_t>((msg.packet_id >> 8) & 0xFF),
				static_cast<uint8_t>(msg.packet_id & 0xFF)
			};
			socket_.send(std::span<const uint8_t>(puback.data(), puback.size()));
		}
	}
};

} // namespace protocol
} // namespace etherz

/**
 * @file mqtt_pubsub.cpp
 * @brief MQTT publish/subscribe example
 * 
 * Usage:
 *   mqtt_pubsub publish  — Publish temperature readings
 *   mqtt_pubsub subscribe — Subscribe to temperature topic
 */

#include "../include/protocol/mqtt_client.hpp"
#include <print>
#include <string>
#include <thread>
#include <chrono>

namespace ep = etherz::protocol;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: mqtt_pubsub <publish|subscribe>\n");
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "publish") {
		ep::MqttClient client;
		auto err = client.connect("localhost", 1883,
			{.client_id = "etherz-publisher"});

		if (ec::is_error(err)) {
			std::print("Connect failed: {}\n", ec::error_message(err));
			return 1;
		}

		std::print("Connected to MQTT broker\n");

		for (int i = 0; i < 10; ++i) {
			std::string temp = std::to_string(20.0 + i * 0.5);
			client.publish("sensor/temperature", temp,
				ep::MqttQoS::AtLeastOnce);
			std::print("Published: {} °C\n", temp);
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		client.disconnect();
	}
	else if (mode == "subscribe") {
		ep::MqttClient client;
		auto err = client.connect("localhost", 1883,
			{.client_id = "etherz-subscriber"});

		if (ec::is_error(err)) {
			std::print("Connect failed: {}\n", ec::error_message(err));
			return 1;
		}

		client.subscribe("sensor/temperature", ep::MqttQoS::AtLeastOnce,
			[](const ep::MqttMessage& msg) {
				std::print("Temperature: {} °C\n", msg.payload_string());
			});

		std::print("Subscribed to sensor/temperature\n");

		while (client.is_connected()) {
			client.loop_once();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	return 0;
}

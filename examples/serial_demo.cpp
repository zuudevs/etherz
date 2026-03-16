/**
 * @file serial_demo.cpp
 * @brief Serial port communication demo
 * 
 * Usage:
 *   serial_demo <port>          — Open port, write and read
 *   serial_demo COM3            — Windows
 *   serial_demo /dev/ttyUSB0    — Linux
 */

#include "../include/net/serial_port.hpp"
#include <print>
#include <string>
#include <array>

namespace en = etherz::net;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::print("Usage: serial_demo <port>\n");
		std::print("  Examples: serial_demo COM3\n");
		std::print("            serial_demo /dev/ttyUSB0\n");
		return 1;
	}

	std::string port_name = argv[1];

	en::SerialPort port;

	// Open port
	if (auto err = port.open(port_name); ec::is_error(err)) {
		std::print("Failed to open {}: {}\n", port_name, ec::error_message(err));
		return 1;
	}
	std::print("Opened serial port: {}\n", port.port_name());

	// Configure: 115200 baud, 8N1
	en::SerialConfig config;
	config.baud_rate = en::BaudRate::B115200;
	config.data_bits = en::DataBits::Eight;
	config.parity = en::Parity::None;
	config.stop_bits = en::StopBits::One;
	config.timeout_ms = 2000;

	if (auto err = port.configure(config); ec::is_error(err)) {
		std::print("Configuration failed: {}\n", ec::error_message(err));
		return 1;
	}
	std::print("Configured: 115200 8N1\n");

	// Send data
	std::string message = "Hello from Etherz!\r\n";
	int written = port.write_string(message);
	std::print("Wrote {} bytes: {}", written, message);

	// Read response
	std::array<uint8_t, 256> buffer{};
	int bytes_read = port.read(buffer);
	if (bytes_read > 0) {
		std::string response(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(bytes_read));
		std::print("Read {} bytes: {}\n", bytes_read, response);
	} else {
		std::print("No response received (timeout)\n");
	}

	port.close();
	std::print("Port closed.\n");

	return 0;
}

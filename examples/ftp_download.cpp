/**
 * @file ftp_download.cpp
 * @brief FTP file download example
 */

#include "../include/protocol/ftp_client.hpp"
#include <print>
#include <fstream>

namespace ep = etherz::protocol;
namespace ec = etherz::core;

int main() {
	ep::FtpClient ftp;

	auto err = ftp.connect("ftp.example.com", 21);
	if (ec::is_error(err)) {
		std::print("Connect failed: {}\n", ec::error_message(err));
		return 1;
	}

	auto login = ftp.login("anonymous", "etherz@example.com");
	if (!login) {
		std::print("Login failed\n");
		return 1;
	}

	std::print("Connected and logged in\n");

	// Print working directory
	if (auto pwd = ftp.pwd()) {
		std::print("CWD: {}\n", *pwd);
	}

	// List directory
	if (auto listing = ftp.list()) {
		std::print("Directory listing:\n{}\n", *listing);
	}

	// Download a file
	if (auto data = ftp.download("README.txt")) {
		std::print("Downloaded {} bytes\n", data->size());
		std::ofstream out("README.txt", std::ios::binary);
		out.write(reinterpret_cast<const char*>(data->data()), data->size());
		std::print("Saved to README.txt\n");
	} else {
		std::print("Download failed\n");
	}

	ftp.disconnect();
	return 0;
}

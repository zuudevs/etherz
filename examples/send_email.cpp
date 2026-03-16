/**
 * @file send_email.cpp
 * @brief SMTP email sending example
 */

#include "../include/protocol/smtp_client.hpp"
#include <print>

namespace ep = etherz::protocol;
namespace ec = etherz::core;

int main() {
	ep::SmtpClient smtp;

	auto err = smtp.connect("smtp.gmail.com", 587);
	if (ec::is_error(err)) {
		std::print("Connect failed: {}\n", ec::error_message(err));
		return 1;
	}

	std::print("Connected to SMTP server\n");

	err = smtp.login("user@gmail.com", "app-password");
	if (ec::is_error(err)) {
		std::print("Auth failed: {}\n", ec::error_message(err));
		return 1;
	}

	std::print("Authenticated\n");

	// Send a simple email
	auto email = ep::EmailMessage::create(
		"user@gmail.com",
		"friend@example.com",
		"Hello from Etherz!",
		"This email was sent using the Etherz C++ networking library.\n\n"
		"Features:\n"
		"- SMTP with AUTH LOGIN\n"
		"- MIME multipart support\n"
		"- Base64 attachment encoding\n");

	auto result = smtp.send(email);
	if (result) {
		std::print("Email sent! Server: {} {}\n",
			result->code, result->message);
	} else {
		std::print("Send failed: {}\n", ec::error_message(result.error()));
	}

	smtp.disconnect();
	return 0;
}

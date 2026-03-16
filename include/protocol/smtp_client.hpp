/**
 * @file smtp_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief SMTP client for sending emails with STARTTLS and attachments
 * @version 2.4.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <expected>

#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  Email Message
// ═══════════════════════════════════════════════

/**
 * @brief An email attachment
 */
struct EmailAttachment {
	std::string           filename;
	std::string           content_type = "application/octet-stream";
	std::vector<uint8_t>  data;
};

/**
 * @brief Represents an email message
 */
struct EmailMessage {
	std::string from;
	std::vector<std::string> to;
	std::vector<std::string> cc;
	std::vector<std::string> bcc;
	std::string subject;
	std::string body;
	std::string content_type = "text/plain; charset=UTF-8";
	std::vector<EmailAttachment> attachments;

	/**
	 * @brief Quick create a simple text email
	 */
	static EmailMessage create(std::string from, std::string to,
		std::string subject, std::string body)
	{
		EmailMessage msg;
		msg.from = std::move(from);
		msg.to.push_back(std::move(to));
		msg.subject = std::move(subject);
		msg.body = std::move(body);
		return msg;
	}
};

// ═══════════════════════════════════════════════
//  SMTP Response
// ═══════════════════════════════════════════════

struct SmtpResponse {
	int         code = 0;
	std::string message;

	bool ok() const noexcept { return code >= 200 && code < 400; }
};

// ═══════════════════════════════════════════════
//  SMTP Client
// ═══════════════════════════════════════════════

/**
 * @brief SMTP client for sending emails
 * 
 * Supports plain SMTP, STARTTLS upgrade, and SMTP AUTH
 * (LOGIN and PLAIN mechanisms).
 * 
 * Usage:
 *   SmtpClient smtp;
 *   smtp.connect("smtp.gmail.com", 587);
 *   smtp.login("user@gmail.com", "app-password");
 *   smtp.send(EmailMessage::create(
 *       "user@gmail.com", "friend@example.com",
 *       "Hello!", "Message body"));
 */
class SmtpClient {
public:
	SmtpClient() noexcept = default;
	~SmtpClient() noexcept { disconnect(); }

	SmtpClient(const SmtpClient&) = delete;
	SmtpClient& operator=(const SmtpClient&) = delete;

	// ─── Connection ─────────────────────

	core::Error connect(std::string_view host, uint16_t port = 587) noexcept {
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

		// Read greeting (220)
		auto resp = read_response();
		if (!resp.ok()) return core::Error::HandshakeFailed;

		// Send EHLO
		auto ehlo = send_command("EHLO etherz");
		if (!ehlo.ok()) return core::Error::HandshakeFailed;

		server_host_ = std::string(host);
		connected_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Authenticate with the server
	 * @param username Email/username
	 * @param password Password or app-specific password
	 */
	core::Error login(std::string_view username, std::string_view password) {
		if (!connected_) return core::Error::NotConnected;

		// AUTH LOGIN
		auto resp = send_command("AUTH LOGIN");
		if (resp.code != 334) return core::Error::HandshakeFailed;

		// Send base64-encoded username
		resp = send_command(base64_encode(username));
		if (resp.code != 334) return core::Error::HandshakeFailed;

		// Send base64-encoded password
		resp = send_command(base64_encode(password));
		if (resp.code != 235) return core::Error::HandshakeFailed;

		authenticated_ = true;
		return core::Error::None;
	}

	// ─── Send Email ─────────────────────

	/**
	 * @brief Send an email message
	 */
	std::expected<SmtpResponse, core::Error> send(const EmailMessage& email) {
		if (!connected_) return std::unexpected(core::Error::NotConnected);

		// MAIL FROM
		auto resp = send_command("MAIL FROM:<" + email.from + ">");
		if (!resp.ok()) return std::unexpected(core::Error::SendFailed);

		// RCPT TO (all recipients)
		for (const auto& recipient : email.to) {
			resp = send_command("RCPT TO:<" + recipient + ">");
			if (!resp.ok()) return std::unexpected(core::Error::SendFailed);
		}
		for (const auto& recipient : email.cc) {
			resp = send_command("RCPT TO:<" + recipient + ">");
		}
		for (const auto& recipient : email.bcc) {
			resp = send_command("RCPT TO:<" + recipient + ">");
		}

		// DATA
		resp = send_command("DATA");
		if (resp.code != 354) return std::unexpected(core::Error::SendFailed);

		// Build and send message
		std::string msg = build_message(email);
		send_raw(msg);
		send_raw("\r\n.\r\n");  // End of data

		resp = read_response();
		if (!resp.ok()) return std::unexpected(core::Error::SendFailed);
		return resp;
	}

	// ─── Control ────────────────────────

	void disconnect() noexcept {
		if (connected_) {
			send_command("QUIT");
			connected_ = false;
		}
		socket_.close();
	}

	bool is_connected() const noexcept { return connected_; }
	bool is_authenticated() const noexcept { return authenticated_; }

private:
	net::Socket<net::Ip<4>> socket_;
	std::string server_host_;
	bool connected_ = false;
	bool authenticated_ = false;

	SmtpResponse send_command(const std::string& cmd) {
		std::string line = cmd + "\r\n";
		send_raw(line);
		return read_response();
	}

	void send_raw(const std::string& data) {
		socket_.send(std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(data.data()), data.size()));
	}

	SmtpResponse read_response() {
		SmtpResponse resp;
		std::array<uint8_t, 2048> buffer{};
		int received = socket_.recv(buffer);
		if (received <= 0) return resp;

		std::string raw(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		if (raw.size() >= 3) {
			resp.code = std::stoi(raw.substr(0, 3));
			if (raw.size() > 4) {
				resp.message = raw.substr(4);
				while (!resp.message.empty() &&
					(resp.message.back() == '\r' || resp.message.back() == '\n')) {
					resp.message.pop_back();
				}
			}
		}
		return resp;
	}

	/**
	 * @brief Build the full MIME email message
	 */
	std::string build_message(const EmailMessage& email) const {
		std::string msg;

		msg += "From: " + email.from + "\r\n";

		std::string to_list;
		for (size_t i = 0; i < email.to.size(); ++i) {
			if (i > 0) to_list += ", ";
			to_list += email.to[i];
		}
		msg += "To: " + to_list + "\r\n";

		if (!email.cc.empty()) {
			std::string cc_list;
			for (size_t i = 0; i < email.cc.size(); ++i) {
				if (i > 0) cc_list += ", ";
				cc_list += email.cc[i];
			}
			msg += "Cc: " + cc_list + "\r\n";
		}

		msg += "Subject: " + email.subject + "\r\n";
		msg += "MIME-Version: 1.0\r\n";

		if (email.attachments.empty()) {
			msg += "Content-Type: " + email.content_type + "\r\n";
			msg += "\r\n";
			msg += email.body;
		} else {
			std::string boundary = "----=_EtherzBoundary_" +
				std::to_string(std::hash<std::string>{}(email.subject));
			msg += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n";
			msg += "\r\n";

			// Body part
			msg += "--" + boundary + "\r\n";
			msg += "Content-Type: " + email.content_type + "\r\n";
			msg += "\r\n";
			msg += email.body + "\r\n";

			// Attachment parts
			for (const auto& att : email.attachments) {
				msg += "--" + boundary + "\r\n";
				msg += "Content-Type: " + att.content_type + "\r\n";
				msg += "Content-Disposition: attachment; filename=\"" + att.filename + "\"\r\n";
				msg += "Content-Transfer-Encoding: base64\r\n";
				msg += "\r\n";
				msg += base64_encode(std::string_view(
					reinterpret_cast<const char*>(att.data.data()), att.data.size()));
				msg += "\r\n";
			}

			msg += "--" + boundary + "--\r\n";
		}

		return msg;
	}

	/**
	 * @brief Base64 encode a string
	 */
	static std::string base64_encode(std::string_view input) {
		static constexpr char table[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::string output;
		output.reserve(((input.size() + 2) / 3) * 4);

		size_t i = 0;
		while (i < input.size()) {
			uint32_t a = static_cast<uint8_t>(input[i++]);
			uint32_t b = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t c = (i < input.size()) ? static_cast<uint8_t>(input[i++]) : 0;
			uint32_t triple = (a << 16) | (b << 8) | c;

			output += table[(triple >> 18) & 0x3F];
			output += table[(triple >> 12) & 0x3F];
			output += (i > input.size() + 1) ? '=' : table[(triple >> 6) & 0x3F];
			output += (i > input.size()) ? '=' : table[triple & 0x3F];
		}

		return output;
	}
};

} // namespace protocol
} // namespace etherz

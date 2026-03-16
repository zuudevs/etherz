#include "test_framework.hpp"
#include "protocol/smtp_client.hpp"

namespace ep = etherz::protocol;

// ─── EmailMessage (v2.4.0) ─────────────

TEST_CASE(email_message_create) {
	auto msg = ep::EmailMessage::create(
		"alice@example.com", "bob@example.com",
		"Test Subject", "Hello Bob!");
	CHECK_EQ(msg.from, std::string("alice@example.com"));
	CHECK_EQ(msg.to.size(), static_cast<size_t>(1));
	CHECK_EQ(msg.to[0], std::string("bob@example.com"));
	CHECK_EQ(msg.subject, std::string("Test Subject"));
	CHECK_EQ(msg.body, std::string("Hello Bob!"));
	CHECK_TRUE(msg.cc.empty());
	CHECK_TRUE(msg.bcc.empty());
	CHECK_TRUE(msg.attachments.empty());
}

TEST_CASE(email_message_content_type_default) {
	ep::EmailMessage msg;
	CHECK_EQ(msg.content_type, std::string("text/plain; charset=UTF-8"));
}

// ─── SmtpResponse ─────────────────────

TEST_CASE(smtp_response_ok_200) {
	ep::SmtpResponse resp;
	resp.code = 250;
	resp.message = "OK";
	CHECK_TRUE(resp.ok());
}

TEST_CASE(smtp_response_ok_354) {
	ep::SmtpResponse resp;
	resp.code = 354;
	resp.message = "Start mail input";
	CHECK_TRUE(resp.ok());
}

TEST_CASE(smtp_response_error_500) {
	ep::SmtpResponse resp;
	resp.code = 550;
	resp.message = "Mailbox not found";
	CHECK_FALSE(resp.ok());
}

TEST_CASE(smtp_response_error_zero) {
	ep::SmtpResponse resp;
	resp.code = 0;
	CHECK_FALSE(resp.ok());
}

// ─── EmailAttachment ──────────────────

TEST_CASE(email_attachment_defaults) {
	ep::EmailAttachment att;
	CHECK_EQ(att.content_type, std::string("application/octet-stream"));
	CHECK_TRUE(att.filename.empty());
	CHECK_TRUE(att.data.empty());
}

// ─── FTP Response (v2.4.0) ────────────

#include "protocol/ftp_client.hpp"

TEST_CASE(ftp_response_ok) {
	ep::FtpResponse resp;
	resp.code = 200;
	CHECK_TRUE(resp.ok());
	CHECK_TRUE(resp.is_positive_completion());
	CHECK_FALSE(resp.is_positive_preliminary());
}

TEST_CASE(ftp_response_preliminary) {
	ep::FtpResponse resp;
	resp.code = 150;
	CHECK_FALSE(resp.ok());
	CHECK_TRUE(resp.is_positive_preliminary());
}

TEST_CASE(ftp_response_intermediate) {
	ep::FtpResponse resp;
	resp.code = 331;
	CHECK_TRUE(resp.ok());
	CHECK_TRUE(resp.is_positive_intermediate());
}

TEST_CASE(ftp_transfer_type_values) {
	CHECK_EQ(static_cast<uint8_t>(ep::FtpTransferType::Ascii), static_cast<uint8_t>('A'));
	CHECK_EQ(static_cast<uint8_t>(ep::FtpTransferType::Binary), static_cast<uint8_t>('I'));
}

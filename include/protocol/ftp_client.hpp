/**
 * @file ftp_client.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief FTP client with login, list, upload, download, passive mode
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
#include <sstream>

#include "../net/socket.hpp"
#include "../net/socket_address.hpp"
#include "../net/internet_protocol.hpp"
#include "../net/dns.hpp"
#include "../core/error.hpp"

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  FTP Response
// ═══════════════════════════════════════════════

struct FtpResponse {
	int         code = 0;
	std::string message;

	bool ok() const noexcept { return code >= 200 && code < 400; }
	bool is_positive_preliminary() const noexcept { return code >= 100 && code < 200; }
	bool is_positive_completion() const noexcept { return code >= 200 && code < 300; }
	bool is_positive_intermediate() const noexcept { return code >= 300 && code < 400; }
};

// ═══════════════════════════════════════════════
//  FTP Transfer Mode
// ═══════════════════════════════════════════════

enum class FtpTransferType : uint8_t {
	Ascii  = 'A',
	Binary = 'I'
};

// ═══════════════════════════════════════════════
//  FTP Client
// ═══════════════════════════════════════════════

/**
 * @brief FTP client for file transfer operations
 * 
 * Supports login, directory listing, file upload/download,
 * and passive mode data connections.
 * 
 * Usage:
 *   FtpClient ftp;
 *   ftp.connect("ftp.example.com");
 *   ftp.login("user", "pass");
 *   auto listing = ftp.list();
 *   auto data = ftp.download("file.txt");
 */
class FtpClient {
public:
	FtpClient() noexcept = default;
	~FtpClient() noexcept { disconnect(); }

	FtpClient(const FtpClient&) = delete;
	FtpClient& operator=(const FtpClient&) = delete;

	// ─── Connection ─────────────────────

	core::Error connect(std::string_view host, uint16_t port = 21) noexcept {
		auto ip = net::Ip<4>(127, 0, 0, 1);
		if (host != "localhost" && host != "127.0.0.1") {
			auto dns = net::Dns::resolve(std::string(host));
			if (dns.success && !dns.ipv4_addresses.empty()) {
				ip = dns.ipv4_addresses[0];
			}
		}

		auto addr = net::SocketAddress<net::Ip<4>>(ip, port);
		if (auto err = control_.create(); core::is_error(err)) return err;
		if (auto err = control_.connect(addr); core::is_error(err)) return err;

		// Read welcome message (220)
		auto resp = read_response();
		if (!resp.ok()) return core::Error::HandshakeFailed;

		connected_ = true;
		return core::Error::None;
	}

	/**
	 * @brief Login with username and password
	 */
	std::expected<FtpResponse, core::Error> login(
		std::string_view user = "anonymous",
		std::string_view pass = "etherz@example.com")
	{
		auto resp = send_command("USER " + std::string(user));
		if (resp.code == 331) {
			resp = send_command("PASS " + std::string(pass));
		}
		if (!resp.ok()) return std::unexpected(core::Error::HandshakeFailed);
		return resp;
	}

	// ─── Directory Operations ───────────

	/**
	 * @brief Get current working directory
	 */
	std::expected<std::string, core::Error> pwd() {
		auto resp = send_command("PWD");
		if (!resp.ok()) return std::unexpected(core::Error::ReceiveFailed);
		// Extract path from response like: 257 "/home/user"
		auto start = resp.message.find('"');
		auto end = resp.message.rfind('"');
		if (start != std::string::npos && end != std::string::npos && end > start) {
			return resp.message.substr(start + 1, end - start - 1);
		}
		return resp.message;
	}

	/**
	 * @brief Change directory
	 */
	std::expected<FtpResponse, core::Error> cd(std::string_view path) {
		auto resp = send_command("CWD " + std::string(path));
		if (!resp.ok()) return std::unexpected(core::Error::ReceiveFailed);
		return resp;
	}

	/**
	 * @brief List directory contents
	 */
	std::expected<std::string, core::Error> list(std::string_view path = "") {
		auto data_sock = enter_passive();
		if (!data_sock) return std::unexpected(data_sock.error());

		std::string cmd = "LIST";
		if (!path.empty()) cmd += " " + std::string(path);
		auto resp = send_command(cmd);
		if (!resp.is_positive_preliminary()) {
			return std::unexpected(core::Error::ReceiveFailed);
		}

		auto listing = read_data_connection(*data_sock);
		data_sock->close();

		read_response();  // 226 Transfer complete
		return listing;
	}

	// ─── File Transfer ──────────────────

	/**
	 * @brief Download a file
	 */
	std::expected<std::vector<uint8_t>, core::Error> download(std::string_view remote_path) {
		send_command("TYPE I");  // Binary mode

		auto data_sock = enter_passive();
		if (!data_sock) return std::unexpected(data_sock.error());

		auto resp = send_command("RETR " + std::string(remote_path));
		if (!resp.is_positive_preliminary()) {
			return std::unexpected(core::Error::ReceiveFailed);
		}

		auto data = read_binary_data(*data_sock);
		data_sock->close();

		read_response();  // 226 Transfer complete
		return data;
	}

	/**
	 * @brief Upload a file
	 */
	std::expected<FtpResponse, core::Error> upload(
		std::string_view remote_path, std::span<const uint8_t> data)
	{
		send_command("TYPE I");

		auto data_sock = enter_passive();
		if (!data_sock) return std::unexpected(data_sock.error());

		auto resp = send_command("STOR " + std::string(remote_path));
		if (!resp.is_positive_preliminary()) {
			return std::unexpected(core::Error::SendFailed);
		}

		data_sock->send(data);
		data_sock->close();

		auto final_resp = read_response();
		return final_resp;
	}

	/**
	 * @brief Delete a file
	 */
	std::expected<FtpResponse, core::Error> remove(std::string_view path) {
		auto resp = send_command("DELE " + std::string(path));
		if (!resp.ok()) return std::unexpected(core::Error::ReceiveFailed);
		return resp;
	}

	/**
	 * @brief Get file size
	 */
	std::expected<size_t, core::Error> size(std::string_view path) {
		auto resp = send_command("SIZE " + std::string(path));
		if (!resp.ok()) return std::unexpected(core::Error::ReceiveFailed);
		return static_cast<size_t>(std::stoull(resp.message));
	}

	// ─── Control ────────────────────────

	void disconnect() noexcept {
		if (connected_) {
			send_command("QUIT");
			connected_ = false;
		}
		control_.close();
	}

	bool is_connected() const noexcept { return connected_; }

private:
	net::Socket<net::Ip<4>> control_;
	bool connected_ = false;

	FtpResponse send_command(const std::string& cmd) {
		std::string line = cmd + "\r\n";
		control_.send(std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(line.data()), line.size()));
		return read_response();
	}

	FtpResponse read_response() {
		FtpResponse resp;
		std::array<uint8_t, 1024> buffer{};
		int received = control_.recv(buffer);
		if (received <= 0) return resp;

		std::string raw(reinterpret_cast<const char*>(buffer.data()),
			static_cast<size_t>(received));

		if (raw.size() >= 3) {
			resp.code = std::stoi(raw.substr(0, 3));
			if (raw.size() > 4) {
				resp.message = raw.substr(4);
				// Trim trailing \r\n
				while (!resp.message.empty() &&
					(resp.message.back() == '\r' || resp.message.back() == '\n')) {
					resp.message.pop_back();
				}
			}
		}
		return resp;
	}

	/**
	 * @brief Enter passive mode and open data connection
	 */
	std::expected<net::Socket<net::Ip<4>>, core::Error> enter_passive() {
		auto resp = send_command("PASV");
		if (!resp.ok()) return std::unexpected(core::Error::ReceiveFailed);

		// Parse: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
		auto start = resp.message.find('(');
		auto end = resp.message.find(')');
		if (start == std::string::npos || end == std::string::npos) {
			return std::unexpected(core::Error::ReceiveFailed);
		}

		std::string nums = resp.message.substr(start + 1, end - start - 1);
		std::vector<int> parts;
		std::stringstream ss(nums);
		std::string token;
		while (std::getline(ss, token, ',')) {
			parts.push_back(std::stoi(token));
		}
		if (parts.size() < 6) return std::unexpected(core::Error::ReceiveFailed);

		auto ip = net::Ip<4>(
			static_cast<uint8_t>(parts[0]), static_cast<uint8_t>(parts[1]),
			static_cast<uint8_t>(parts[2]), static_cast<uint8_t>(parts[3]));
		uint16_t port = static_cast<uint16_t>(parts[4] * 256 + parts[5]);

		net::Socket<net::Ip<4>> data_sock;
		auto addr = net::SocketAddress<net::Ip<4>>(ip, port);
		if (auto err = data_sock.create(); core::is_error(err))
			return std::unexpected(err);
		if (auto err = data_sock.connect(addr); core::is_error(err))
			return std::unexpected(err);

		return data_sock;
	}

	std::string read_data_connection(net::Socket<net::Ip<4>>& sock) {
		std::string result;
		std::array<uint8_t, 4096> buffer{};
		while (true) {
			int received = sock.recv(buffer);
			if (received <= 0) break;
			result.append(reinterpret_cast<const char*>(buffer.data()),
				static_cast<size_t>(received));
		}
		return result;
	}

	std::vector<uint8_t> read_binary_data(net::Socket<net::Ip<4>>& sock) {
		std::vector<uint8_t> result;
		std::array<uint8_t, 4096> buffer{};
		while (true) {
			int received = sock.recv(buffer);
			if (received <= 0) break;
			result.insert(result.end(), buffer.begin(),
				buffer.begin() + received);
		}
		return result;
	}
};

} // namespace protocol
} // namespace etherz

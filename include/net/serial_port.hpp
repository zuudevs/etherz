/**
 * @file serial_port.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Serial port communication with platform abstraction
 * @version 1.5.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <expected>

#include "../core/error.hpp"

#ifdef _WIN32
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <termios.h>
	#include <cstring>
#endif

namespace etherz {
namespace net {

// ═══════════════════════════════════════════════
//  Serial Port Configuration
// ═══════════════════════════════════════════════

/**
 * @brief Baud rate presets
 */
enum class BaudRate : uint32_t {
	B9600   = 9600,
	B19200  = 19200,
	B38400  = 38400,
	B57600  = 57600,
	B115200 = 115200,
	B230400 = 230400,
	B460800 = 460800,
	B921600 = 921600
};

/**
 * @brief Data bits configuration
 */
enum class DataBits : uint8_t {
	Five  = 5,
	Six   = 6,
	Seven = 7,
	Eight = 8
};

/**
 * @brief Parity mode
 */
enum class Parity : uint8_t {
	None,
	Odd,
	Even,
	Mark,
	Space
};

/**
 * @brief Stop bits configuration
 */
enum class StopBits : uint8_t {
	One,
	OnePointFive,
	Two
};

/**
 * @brief Serial port configuration
 */
struct SerialConfig {
	BaudRate baud_rate = BaudRate::B115200;
	DataBits data_bits = DataBits::Eight;
	Parity   parity    = Parity::None;
	StopBits stop_bits = StopBits::One;
	uint32_t timeout_ms = 1000;  // Read timeout
};

// ═══════════════════════════════════════════════
//  Serial Port
// ═══════════════════════════════════════════════

/**
 * @brief Cross-platform serial port communication
 * 
 * Opens and configures a serial port (COM port on Windows,
 * /dev/tty* on POSIX) for reading and writing.
 * 
 * Usage:
 *   SerialPort port;
 *   port.open("COM3");            // Windows
 *   port.open("/dev/ttyUSB0");    // Linux
 *   port.configure({.baud_rate = BaudRate::B115200});
 *   port.write(data);
 *   auto result = port.read(buffer);
 */
class SerialPort {
public:
	SerialPort() noexcept = default;
	~SerialPort() noexcept { close(); }

	// Non-copyable, movable
	SerialPort(const SerialPort&) = delete;
	SerialPort& operator=(const SerialPort&) = delete;

	SerialPort(SerialPort&& other) noexcept
#ifdef _WIN32
		: handle_(other.handle_)
#else
		: fd_(other.fd_)
#endif
		, port_name_(std::move(other.port_name_))
	{
#ifdef _WIN32
		other.handle_ = INVALID_HANDLE_VALUE;
#else
		other.fd_ = -1;
#endif
	}

	SerialPort& operator=(SerialPort&& other) noexcept {
		if (this != &other) {
			close();
#ifdef _WIN32
			handle_ = other.handle_;
			other.handle_ = INVALID_HANDLE_VALUE;
#else
			fd_ = other.fd_;
			other.fd_ = -1;
#endif
			port_name_ = std::move(other.port_name_);
		}
		return *this;
	}

	// ─── Lifecycle ──────────────────────

	/**
	 * @brief Open a serial port
	 * @param port Port name ("COM3", "/dev/ttyUSB0", etc.)
	 */
	core::Error open(std::string_view port) noexcept {
		port_name_ = std::string(port);

#ifdef _WIN32
		// On Windows, use CreateFile
		std::string full_path = "\\\\.\\" + port_name_;
		handle_ = CreateFileA(
			full_path.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,        // No sharing
			nullptr,  // Default security
			OPEN_EXISTING,
			0,        // No overlapped I/O
			nullptr
		);
		if (handle_ == INVALID_HANDLE_VALUE) {
			return core::Error::SocketCreationFailed;
		}
#else
		// On POSIX, use open()
		fd_ = ::open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
		if (fd_ < 0) {
			return core::Error::SocketCreationFailed;
		}
		// Reset to blocking mode
		int flags = fcntl(fd_, F_GETFL, 0);
		fcntl(fd_, F_SETFL, flags & ~O_NDELAY);
#endif

		return core::Error::None;
	}

	/**
	 * @brief Configure the serial port
	 * @param config Serial port configuration
	 */
	core::Error configure(const SerialConfig& config) noexcept {
		if (!is_open()) return core::Error::SocketClosed;

#ifdef _WIN32
		// Configure DCB
		DCB dcb{};
		dcb.DCBlength = sizeof(DCB);
		if (!GetCommState(handle_, &dcb)) return core::Error::OptionFailed;

		dcb.BaudRate = static_cast<DWORD>(config.baud_rate);
		dcb.ByteSize = static_cast<BYTE>(config.data_bits);

		switch (config.parity) {
			case Parity::None:  dcb.Parity = NOPARITY; break;
			case Parity::Odd:   dcb.Parity = ODDPARITY; break;
			case Parity::Even:  dcb.Parity = EVENPARITY; break;
			case Parity::Mark:  dcb.Parity = MARKPARITY; break;
			case Parity::Space: dcb.Parity = SPACEPARITY; break;
		}

		switch (config.stop_bits) {
			case StopBits::One:          dcb.StopBits = ONESTOPBIT; break;
			case StopBits::OnePointFive: dcb.StopBits = ONE5STOPBITS; break;
			case StopBits::Two:          dcb.StopBits = TWOSTOPBITS; break;
		}

		dcb.fBinary = TRUE;
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		dcb.fRtsControl = RTS_CONTROL_ENABLE;

		if (!SetCommState(handle_, &dcb)) return core::Error::OptionFailed;

		// Configure timeouts
		COMMTIMEOUTS timeouts{};
		timeouts.ReadIntervalTimeout = 50;
		timeouts.ReadTotalTimeoutConstant = config.timeout_ms;
		timeouts.ReadTotalTimeoutMultiplier = 10;
		timeouts.WriteTotalTimeoutConstant = config.timeout_ms;
		timeouts.WriteTotalTimeoutMultiplier = 10;

		if (!SetCommTimeouts(handle_, &timeouts)) return core::Error::OptionFailed;
#else
		// POSIX: Configure termios
		struct termios tty{};
		if (tcgetattr(fd_, &tty) != 0) return core::Error::OptionFailed;

		// Set baud rate
		speed_t speed;
		switch (config.baud_rate) {
			case BaudRate::B9600:   speed = B9600; break;
			case BaudRate::B19200:  speed = B19200; break;
			case BaudRate::B38400:  speed = B38400; break;
			case BaudRate::B57600:  speed = B57600; break;
			case BaudRate::B115200: speed = B115200; break;
			case BaudRate::B230400: speed = B230400; break;
			default: speed = B115200; break;
		}
		cfsetispeed(&tty, speed);
		cfsetospeed(&tty, speed);

		// Data bits
		tty.c_cflag &= ~CSIZE;
		switch (config.data_bits) {
			case DataBits::Five:  tty.c_cflag |= CS5; break;
			case DataBits::Six:   tty.c_cflag |= CS6; break;
			case DataBits::Seven: tty.c_cflag |= CS7; break;
			case DataBits::Eight: tty.c_cflag |= CS8; break;
		}

		// Parity
		switch (config.parity) {
			case Parity::None:
				tty.c_cflag &= ~PARENB;
				break;
			case Parity::Odd:
				tty.c_cflag |= PARENB | PARODD;
				break;
			case Parity::Even:
				tty.c_cflag |= PARENB;
				tty.c_cflag &= ~PARODD;
				break;
			default:
				tty.c_cflag &= ~PARENB;
				break;
		}

		// Stop bits
		if (config.stop_bits == StopBits::Two) {
			tty.c_cflag |= CSTOPB;
		} else {
			tty.c_cflag &= ~CSTOPB;
		}

		// Enable receiver, local mode
		tty.c_cflag |= CLOCAL | CREAD;

		// Raw input mode
		tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
		tty.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR);
		tty.c_oflag &= ~OPOST;

		// Read timeout
		tty.c_cc[VTIME] = static_cast<cc_t>(config.timeout_ms / 100);
		tty.c_cc[VMIN] = 0;

		if (tcsetattr(fd_, TCSANOW, &tty) != 0) return core::Error::OptionFailed;
#endif

		return core::Error::None;
	}

	// ─── Data Transfer ──────────────────

	/**
	 * @brief Write data to the serial port
	 * @return Number of bytes written, or -1 on error
	 */
	int write(std::span<const uint8_t> data) noexcept {
		if (!is_open()) return -1;

#ifdef _WIN32
		DWORD bytes_written = 0;
		if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()),
			&bytes_written, nullptr)) {
			return -1;
		}
		return static_cast<int>(bytes_written);
#else
		auto result = ::write(fd_, data.data(), data.size());
		return static_cast<int>(result);
#endif
	}

	/**
	 * @brief Read data from the serial port
	 * @return Number of bytes read, or -1 on error
	 */
	int read(std::span<uint8_t> buffer) noexcept {
		if (!is_open()) return -1;

#ifdef _WIN32
		DWORD bytes_read = 0;
		if (!ReadFile(handle_, buffer.data(), static_cast<DWORD>(buffer.size()),
			&bytes_read, nullptr)) {
			return -1;
		}
		return static_cast<int>(bytes_read);
#else
		auto result = ::read(fd_, buffer.data(), buffer.size());
		return static_cast<int>(result);
#endif
	}

	/**
	 * @brief Write a string to the serial port
	 */
	int write_string(std::string_view str) noexcept {
		return write(std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(str.data()), str.size()));
	}

	// ─── Control ────────────────────────

	/**
	 * @brief Flush the serial port buffers
	 */
	core::Error flush() noexcept {
		if (!is_open()) return core::Error::SocketClosed;
#ifdef _WIN32
		if (!FlushFileBuffers(handle_)) return core::Error::OptionFailed;
#else
		if (tcdrain(fd_) != 0) return core::Error::OptionFailed;
#endif
		return core::Error::None;
	}

	/**
	 * @brief Clear receive and transmit buffers
	 */
	core::Error purge() noexcept {
		if (!is_open()) return core::Error::SocketClosed;
#ifdef _WIN32
		if (!PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR))
			return core::Error::OptionFailed;
#else
		if (tcflush(fd_, TCIOFLUSH) != 0) return core::Error::OptionFailed;
#endif
		return core::Error::None;
	}

	void close() noexcept {
#ifdef _WIN32
		if (handle_ != INVALID_HANDLE_VALUE) {
			CloseHandle(handle_);
			handle_ = INVALID_HANDLE_VALUE;
		}
#else
		if (fd_ >= 0) {
			::close(fd_);
			fd_ = -1;
		}
#endif
		port_name_.clear();
	}

	// ─── Queries ────────────────────────

	bool is_open() const noexcept {
#ifdef _WIN32
		return handle_ != INVALID_HANDLE_VALUE;
#else
		return fd_ >= 0;
#endif
	}

	const std::string& port_name() const noexcept { return port_name_; }

#ifdef _WIN32
	HANDLE native_handle() const noexcept { return handle_; }
#else
	int native_handle() const noexcept { return fd_; }
#endif

private:
#ifdef _WIN32
	HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
	int fd_ = -1;
#endif
	std::string port_name_;
};

} // namespace net
} // namespace etherz

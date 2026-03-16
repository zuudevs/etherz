/**
 * @file compressed_transfer.cpp
 * @brief File transfer with compression example
 * 
 * Demonstrates CompressedSocket for efficient data transfer.
 */

#include "../include/net/compressed_socket.hpp"
#include "../include/net/compression.hpp"
#include "../include/net/stream.hpp"
#include "../include/net/pipe.hpp"
#include "../include/net/internet_protocol.hpp"
#include <print>
#include <string>
#include <vector>

namespace en = etherz::net;
namespace ec = etherz::core;

int main(int argc, char* argv[]) {
	// Demo 1: Compression codec
	std::string text =
		"The quick brown fox jumps over the lazy dog. "
		"The quick brown fox jumps over the lazy dog. "
		"The quick brown fox jumps over the lazy dog. ";

	auto input = std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(text.data()), text.size());

	auto compressed = en::compress(input, en::CompressionType::Deflate);
	auto decompressed = en::decompress(compressed, en::CompressionType::Deflate);

	std::print("=== Compression Demo ===\n");
	std::print("Original:     {} bytes\n", text.size());
	std::print("Compressed:   {} bytes\n", compressed.size());
	std::print("Ratio:        {:.1f}%\n",
		en::compression_ratio(text.size(), compressed.size()) * 100);
	std::print("Decompressed: {} bytes\n", decompressed.size());

	std::string restored(reinterpret_cast<const char*>(decompressed.data()),
		decompressed.size());
	std::print("Match: {}\n", restored == text ? "YES" : "NO");

	// Demo 2: ByteStream ring buffer
	std::print("\n=== ByteStream Demo ===\n");
	en::ByteStream stream(256);
	std::string msg = "Hello, ByteStream!";
	stream.write(std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

	std::print("Written: {} bytes\n", stream.total_written());
	std::print("Available: {} bytes\n", stream.available_read());

	std::array<uint8_t, 256> buf{};
	size_t n = stream.read(buf);
	std::print("Read: {}\n", std::string(reinterpret_cast<char*>(buf.data()), n));

	// Demo 3: Pipe
	std::print("\n=== Pipe Demo ===\n");
	en::Pipe pipe(1024);
	auto& [a, b] = pipe.ends();

	std::string ping = "PING";
	a.write(std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(ping.data()), ping.size()));

	std::array<uint8_t, 64> pipe_buf{};
	size_t read = b.read(pipe_buf);
	std::print("B received: {}\n",
		std::string(reinterpret_cast<char*>(pipe_buf.data()), read));

	std::string pong = "PONG";
	b.write(std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(pong.data()), pong.size()));

	read = a.read(pipe_buf);
	std::print("A received: {}\n",
		std::string(reinterpret_cast<char*>(pipe_buf.data()), read));

	// Demo 4: CompressedSocket (standalone, no server needed)
	std::print("\n=== CompressedSocket ===\n");
	std::print("To use CompressedSocket:\n");
	std::print("  CompressedSocket<Ip<4>> sock(CompressionType::Deflate);\n");
	std::print("  sock.create();\n");
	std::print("  sock.connect(addr);\n");
	std::print("  sock.send(large_data); // Compressed on wire\n");
	std::print("  sock.recv(buffer);     // Decompressed automatically\n");

	return 0;
}

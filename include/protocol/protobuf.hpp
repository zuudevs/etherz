/**
 * @file protobuf.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief Lightweight Protocol Buffers wire format codec
 * @version 2.0.0
 * @date 2026-03-16
 * 
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <variant>
#include <unordered_map>

namespace etherz {
namespace protocol {

// ═══════════════════════════════════════════════
//  Protocol Buffers Wire Types
// ═══════════════════════════════════════════════

/**
 * @brief Protobuf wire types (encoding spec)
 */
enum class ProtoWireType : uint8_t {
	Varint          = 0,   // int32, int64, uint32, uint64, sint32, sint64, bool, enum
	Fixed64         = 1,   // fixed64, sfixed64, double
	LengthDelimited = 2,   // string, bytes, embedded messages, packed repeated fields
	Fixed32         = 5    // fixed32, sfixed32, float
};

// ═══════════════════════════════════════════════
//  Protobuf Field Value
// ═══════════════════════════════════════════════

/**
 * @brief A protobuf field value (variant of possible types)
 */
using ProtoValue = std::variant<
	uint64_t,              // Varint
	double,                // Fixed64 (as double)
	std::string,           // LengthDelimited (string/bytes)
	float,                 // Fixed32 (as float)
	std::vector<uint8_t>   // Raw bytes
>;

/**
 * @brief A single protobuf field
 */
struct ProtoField {
	uint32_t      field_number = 0;
	ProtoWireType wire_type    = ProtoWireType::Varint;
	ProtoValue    value;
};

// ═══════════════════════════════════════════════
//  Protobuf Message
// ═══════════════════════════════════════════════

/**
 * @brief A decoded protobuf message (field number → values)
 * 
 * Provides a dynamic, schema-less view of protobuf messages.
 * Fields are accessed by field number. Repeated fields are
 * stored as multiple entries with the same field number.
 */
class ProtoMessage {
public:
	ProtoMessage() = default;

	// ─── Field Setters ──────────────────

	/**
	 * @brief Set a varint field (int32, int64, uint32, uint64, bool, enum)
	 */
	void set_varint(uint32_t field_number, uint64_t value) {
		fields_.push_back({field_number, ProtoWireType::Varint, value});
	}

	/**
	 * @brief Set a string field
	 */
	void set_string(uint32_t field_number, std::string value) {
		fields_.push_back({field_number, ProtoWireType::LengthDelimited, std::move(value)});
	}

	/**
	 * @brief Set a bytes field
	 */
	void set_bytes(uint32_t field_number, std::vector<uint8_t> value) {
		fields_.push_back({field_number, ProtoWireType::LengthDelimited, std::move(value)});
	}

	/**
	 * @brief Set a double field (fixed64)
	 */
	void set_double(uint32_t field_number, double value) {
		fields_.push_back({field_number, ProtoWireType::Fixed64, value});
	}

	/**
	 * @brief Set a float field (fixed32)
	 */
	void set_float(uint32_t field_number, float value) {
		fields_.push_back({field_number, ProtoWireType::Fixed32, value});
	}

	/**
	 * @brief Set an embedded message
	 */
	void set_message(uint32_t field_number, const ProtoMessage& msg) {
		auto encoded = msg.serialize();
		set_bytes(field_number, encoded);
	}

	// ─── Field Getters ──────────────────

	/**
	 * @brief Get a varint field value
	 */
	uint64_t get_varint(uint32_t field_number, uint64_t default_val = 0) const {
		for (const auto& f : fields_) {
			if (f.field_number == field_number && f.wire_type == ProtoWireType::Varint) {
				return std::get<uint64_t>(f.value);
			}
		}
		return default_val;
	}

	/**
	 * @brief Get a string field value
	 */
	std::string get_string(uint32_t field_number) const {
		for (const auto& f : fields_) {
			if (f.field_number == field_number && f.wire_type == ProtoWireType::LengthDelimited) {
				if (std::holds_alternative<std::string>(f.value)) {
					return std::get<std::string>(f.value);
				}
			}
		}
		return "";
	}

	/**
	 * @brief Get a bytes field value
	 */
	std::vector<uint8_t> get_bytes(uint32_t field_number) const {
		for (const auto& f : fields_) {
			if (f.field_number == field_number && f.wire_type == ProtoWireType::LengthDelimited) {
				if (std::holds_alternative<std::vector<uint8_t>>(f.value)) {
					return std::get<std::vector<uint8_t>>(f.value);
				}
			}
		}
		return {};
	}

	bool has_field(uint32_t field_number) const {
		for (const auto& f : fields_) {
			if (f.field_number == field_number) return true;
		}
		return false;
	}

	const std::vector<ProtoField>& fields() const noexcept { return fields_; }

	// ─── Serialization ──────────────────

	/**
	 * @brief Serialize to protobuf wire format
	 */
	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> out;
		for (const auto& field : fields_) {
			// Encode tag: (field_number << 3) | wire_type
			uint32_t tag = (field.field_number << 3) | static_cast<uint32_t>(field.wire_type);
			encode_varint(out, tag);

			switch (field.wire_type) {
				case ProtoWireType::Varint:
					encode_varint(out, std::get<uint64_t>(field.value));
					break;

				case ProtoWireType::Fixed64: {
					double dval = std::get<double>(field.value);
					uint64_t bits;
					std::memcpy(&bits, &dval, sizeof(bits));
					for (int i = 0; i < 8; ++i) {
						out.push_back(static_cast<uint8_t>(bits & 0xFF));
						bits >>= 8;
					}
					break;
				}

				case ProtoWireType::LengthDelimited: {
					if (std::holds_alternative<std::string>(field.value)) {
						const auto& str = std::get<std::string>(field.value);
						encode_varint(out, static_cast<uint64_t>(str.size()));
						out.insert(out.end(), str.begin(), str.end());
					} else if (std::holds_alternative<std::vector<uint8_t>>(field.value)) {
						const auto& bytes = std::get<std::vector<uint8_t>>(field.value);
						encode_varint(out, static_cast<uint64_t>(bytes.size()));
						out.insert(out.end(), bytes.begin(), bytes.end());
					}
					break;
				}

				case ProtoWireType::Fixed32: {
					float fval = std::get<float>(field.value);
					uint32_t bits;
					std::memcpy(&bits, &fval, sizeof(bits));
					for (int i = 0; i < 4; ++i) {
						out.push_back(static_cast<uint8_t>(bits & 0xFF));
						bits >>= 8;
					}
					break;
				}
			}
		}
		return out;
	}

	/**
	 * @brief Deserialize from protobuf wire format
	 */
	static ProtoMessage deserialize(std::span<const uint8_t> data) {
		ProtoMessage msg;
		size_t pos = 0;

		while (pos < data.size()) {
			uint64_t tag = decode_varint(data, pos);
			uint32_t field_number = static_cast<uint32_t>(tag >> 3);
			auto wire_type = static_cast<ProtoWireType>(tag & 0x07);

			ProtoField field;
			field.field_number = field_number;
			field.wire_type = wire_type;

			switch (wire_type) {
				case ProtoWireType::Varint:
					field.value = decode_varint(data, pos);
					break;

				case ProtoWireType::Fixed64: {
					if (pos + 8 > data.size()) return msg;
					uint64_t bits = 0;
					for (int i = 7; i >= 0; --i) {
						bits = (bits << 8) | data[pos + i];
					}
					pos += 8;
					double dval;
					std::memcpy(&dval, &bits, sizeof(dval));
					field.value = dval;
					break;
				}

				case ProtoWireType::LengthDelimited: {
					uint64_t length = decode_varint(data, pos);
					if (pos + length > data.size()) return msg;
					// Store as string (caller can interpret as bytes or embedded message)
					field.value = std::string(
						reinterpret_cast<const char*>(data.data() + pos),
						static_cast<size_t>(length));
					pos += static_cast<size_t>(length);
					break;
				}

				case ProtoWireType::Fixed32: {
					if (pos + 4 > data.size()) return msg;
					uint32_t bits = 0;
					for (int i = 3; i >= 0; --i) {
						bits = (bits << 8) | data[pos + i];
					}
					pos += 4;
					float fval;
					std::memcpy(&fval, &bits, sizeof(fval));
					field.value = fval;
					break;
				}

				default:
					return msg;  // Unknown wire type
			}

			msg.fields_.push_back(std::move(field));
		}

		return msg;
	}

private:
	std::vector<ProtoField> fields_;

	static void encode_varint(std::vector<uint8_t>& out, uint64_t value) {
		while (value >= 0x80) {
			out.push_back(static_cast<uint8_t>((value & 0x7F) | 0x80));
			value >>= 7;
		}
		out.push_back(static_cast<uint8_t>(value));
	}

	static uint64_t decode_varint(std::span<const uint8_t> data, size_t& pos) {
		uint64_t result = 0;
		uint32_t shift = 0;
		while (pos < data.size()) {
			uint8_t byte = data[pos++];
			result |= static_cast<uint64_t>(byte & 0x7F) << shift;
			if ((byte & 0x80) == 0) break;
			shift += 7;
		}
		return result;
	}
};

// ═══════════════════════════════════════════════
//  gRPC Length-Prefixed Message Framing
// ═══════════════════════════════════════════════

/**
 * @brief Encode a protobuf message with gRPC length-prefixed framing
 * 
 * Format: Compressed-Flag (1 byte) | Message-Length (4 bytes BE) | Message
 */
inline std::vector<uint8_t> grpc_encode_message(const std::vector<uint8_t>& message,
	bool compressed = false)
{
	std::vector<uint8_t> out;
	out.push_back(compressed ? 1 : 0);
	uint32_t len = static_cast<uint32_t>(message.size());
	out.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
	out.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
	out.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
	out.push_back(static_cast<uint8_t>(len & 0xFF));
	out.insert(out.end(), message.begin(), message.end());
	return out;
}

/**
 * @brief Decode a gRPC length-prefixed message
 * @param data Input data
 * @param compressed Output: whether the message is compressed
 * @return The decoded protobuf message bytes
 */
inline std::vector<uint8_t> grpc_decode_message(std::span<const uint8_t> data,
	bool& compressed)
{
	if (data.size() < 5) return {};
	compressed = (data[0] != 0);
	uint32_t len = (static_cast<uint32_t>(data[1]) << 24)
	             | (static_cast<uint32_t>(data[2]) << 16)
	             | (static_cast<uint32_t>(data[3]) << 8)
	             | static_cast<uint32_t>(data[4]);
	if (data.size() < 5 + len) return {};
	return std::vector<uint8_t>(data.begin() + 5, data.begin() + 5 + len);
}

} // namespace protocol
} // namespace etherz

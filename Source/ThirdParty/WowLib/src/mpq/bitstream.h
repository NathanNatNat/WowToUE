/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <span>
#include <stdexcept>
#include <format>

namespace mpq {

/**
 * Bit-level reader over a byte buffer.
 *
 * JS equivalent: class BitStream — module.exports = BitStream
 */
class BitStream {
public:
	/**
	 * Construct a BitStream from a byte span.
	 * @param data Non-owning view of the byte data to read from.
	 */
	explicit BitStream(std::span<const uint8_t> data);

	/** Current byte position in the stream. */
	size_t bytePosition() const;

	/** Total length of the underlying data in bytes. */
	size_t length() const;

	/**
	 * Read up to 16 bits from the stream.
	 * @param bitCount Number of bits to read (max 16).
	 * @return The bits read, or -1 if not enough data.
	 */
	int readBits(int bitCount);

	/**
	 * Peek at the next byte without advancing the stream.
	 * @return The next byte value, or -1 if not enough data.
	 */
	int peekByte();

	/**
	 * Ensure at least bitCount bits are available in the buffer.
	 * @param bitCount Number of bits required.
	 * @return true if enough bits are available, false otherwise.
	 */
	bool ensureBits(int bitCount);

	/**
	 * Discard bitCount bits from the current buffer.
	 * @param bitCount Number of bits to discard.
	 */
	void wasteBits(int bitCount);

private:
	std::span<const uint8_t> data;
	size_t position;
	uint32_t current;
	int bitCount_;
};

} // namespace mpq

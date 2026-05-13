/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "bitstream.h"

namespace mpq {

BitStream::BitStream(std::span<const uint8_t> data)
	: data(data), position(0), current(0), bitCount_(0) {}

size_t BitStream::bytePosition() const {
	return position;
}

size_t BitStream::length() const {
	return data.size();
}

int BitStream::readBits(int bitCount) {
	if (bitCount > 16)
		throw std::runtime_error(std::format("maximum bitCount is 16, got {}", bitCount));

	if (!ensureBits(bitCount))
		return -1;

	const int mask = (1 << bitCount) - 1;
	const int result = static_cast<int>(current & mask);

	wasteBits(bitCount);
	return result;
}

int BitStream::peekByte() {
	if (!ensureBits(8))
		return -1;

	return static_cast<int>(current & 0xFF);
}

bool BitStream::ensureBits(int bitCount) {
	while (bitCount_ < bitCount) {
		if (position >= data.size())
			return false;

		current |= static_cast<uint32_t>(data[position++]) << bitCount_;
		bitCount_ += 8;
	}
	return true;
}

void BitStream::wasteBits(int bitCount) {
	current >>= bitCount;
	bitCount_ -= bitCount;
}

} // namespace mpq

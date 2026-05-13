/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	Based off original works by Dmitry Chestnykh <dmitry@codingrobots.com>
	License: MIT
 */
#include "salsa20.h"
#include "../buffer.h"

#include <stdexcept>

namespace {

constexpr std::array<uint32_t, 4> SIGMA_32 = { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574 };
constexpr std::array<uint32_t, 4> SIGMA_16 = { 0x61707865, 0x3120646e, 0x79622d36, 0x6b206574 };

inline uint8_t hexCharToNibble(char c) {
	if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
	if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
	if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
	throw std::runtime_error("Invalid hex character");
}

std::vector<uint8_t> hexToBytes(std::string_view hex) {
	std::vector<uint8_t> bytes;
	bytes.reserve(hex.size() / 2);
	for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
		bytes.push_back(static_cast<uint8_t>((hexCharToNibble(hex[i]) << 4) | hexCharToNibble(hex[i + 1])));
	return bytes;
}

inline uint32_t rotl32(uint32_t v, int n) {
	return (v << n) | (v >> (32 - n));
}

} // anonymous namespace

namespace casc {

Salsa20::Salsa20(std::span<const uint8_t> nonce, std::string_view key, int rounds)
	: rounds_(rounds), blockUsed_(64) {
	if (nonce.size() != 8)
		throw std::runtime_error("Unexpected nonce length. 8 bytes expected, got " + std::to_string(nonce.size()));

	if (key.size() != 32 && key.size() != 64)
		throw std::runtime_error("Unexpected key length. 16 or 32 bytes expected, got " + std::to_string(key.size()));

	sigma_ = (key.size() == 32) ? SIGMA_16 : SIGMA_32;

	keyWords_ = {};
	nonceWords_ = { 0, 0 };
	counter_ = { 0, 0 };
	block_ = {};

	setKey(hexToBytes(key));
	setNonce(nonce);
}

void Salsa20::setKey(std::vector<uint8_t> key) {
	// Expand 16-byte (4-word) key into a 32-byte (8-word) key.
	if (key.size() == 16) {
		key.resize(32);
		for (int i = 0; i < 16; i++)
			key[16 + i] = key[i];
	}

	for (int i = 0, j = 0; i < 8; i++, j += 4)
		keyWords_[i] = (key[j] & 0xFF) | ((key[j + 1] & 0xFF) << 8) | ((key[j + 2] & 0xFF) << 16) | ((key[j + 3] & 0xFF) << 24);

	_reset();
}

void Salsa20::setNonce(std::span<const uint8_t> nonce) {
	nonceWords_[0] = (nonce[0] & 0xFF) | ((nonce[1] & 0xFF) << 8) | ((nonce[2] & 0xFF) << 16) | ((nonce[3] & 0xFF) << 24);
	nonceWords_[1] = (nonce[4] & 0xFF) | ((nonce[5] & 0xFF) << 8) | ((nonce[6] & 0xFF) << 16) | ((nonce[7] & 0xFF) << 24);

	_reset();
}

BufferWrapper Salsa20::getBytes(size_t byteCount) {
	BufferWrapper out = BufferWrapper::alloc(byteCount);
	for (size_t i = 0; i < byteCount; i++) {
		if (blockUsed_ == 64) {
			_generateBlock();
			_increment();
			blockUsed_ = 0;
		}

		out.writeUInt8(block_[blockUsed_]);
		blockUsed_++;
	}

	out.seek(0);
	return out;
}

BufferWrapper Salsa20::process(BufferWrapper& buf) {
	BufferWrapper out = BufferWrapper::alloc(buf.byteLength());
	BufferWrapper bytes = getBytes(buf.byteLength());

	buf.seek(0);
	for (size_t i = 0, n = buf.byteLength(); i < n; i++)
		out.writeUInt8(bytes.readUInt8() ^ buf.readUInt8());

	out.seek(0);
	return out;
}

void Salsa20::_reset() {
	counter_[0] = 0;
	counter_[1] = 0;

	blockUsed_ = 64;
}

void Salsa20::_increment() {
	counter_[0] = (counter_[0] + 1) & 0xffffffff;
	if (counter_[0] == 0)
		counter_[1] = (counter_[1] + 1) & 0xffffffff;
}

void Salsa20::_generateBlock() {
	const uint32_t j0 = sigma_[0],
		j1 = keyWords_[0],
		j2 = keyWords_[1],
		j3 = keyWords_[2],
		j4 = keyWords_[3],
		j5 = sigma_[1],
		j6 = nonceWords_[0],
		j7 = nonceWords_[1],
		j8 = counter_[0],
		j9 = counter_[1],
		j10 = sigma_[2],
		j11 = keyWords_[4],
		j12 = keyWords_[5],
		j13 = keyWords_[6],
		j14 = keyWords_[7],
		j15 = sigma_[3];

	uint32_t x0 = j0, x1 = j1, x2 = j2, x3 = j3, x4 = j4, x5 = j5, x6 = j6, x7 = j7,
		x8 = j8, x9 = j9, x10 = j10, x11 = j11, x12 = j12, x13 = j13, x14 = j14, x15 = j15;

	uint32_t u;
	for (int i = 0, n = rounds_; i < n; i += 2) {
		u = x0 + x12;
		x4 ^= rotl32(u, 7);
		u = x4 + x0;
		x8 ^= rotl32(u, 9);
		u = x8 + x4;
		x12 ^= rotl32(u, 13);
		u = x12 + x8;
		x0 ^= rotl32(u, 18);

		u = x5 + x1;
		x9 ^= rotl32(u, 7);
		u = x9 + x5;
		x13 ^= rotl32(u, 9);
		u = x13 + x9;
		x1 ^= rotl32(u, 13);
		u = x1 + x13;
		x5 ^= rotl32(u, 18);

		u = x10 + x6;
		x14 ^= rotl32(u, 7);
		u = x14 + x10;
		x2 ^= rotl32(u, 9);
		u = x2 + x14;
		x6 ^= rotl32(u, 13);
		u = x6 + x2;
		x10 ^= rotl32(u, 18);

		u = x15 + x11;
		x3 ^= rotl32(u, 7);
		u = x3 + x15;
		x7 ^= rotl32(u, 9);
		u = x7 + x3;
		x11 ^= rotl32(u, 13);
		u = x11 + x7;
		x15 ^= rotl32(u, 18);

		u = x0 + x3;
		x1 ^= rotl32(u, 7);
		u = x1 + x0;
		x2 ^= rotl32(u, 9);
		u = x2 + x1;
		x3 ^= rotl32(u, 13);
		u = x3 + x2;
		x0 ^= rotl32(u, 18);

		u = x5 + x4;
		x6 ^= rotl32(u, 7);
		u = x6 + x5;
		x7 ^= rotl32(u, 9);
		u = x7 + x6;
		x4 ^= rotl32(u, 13);
		u = x4 + x7;
		x5 ^= rotl32(u, 18);

		u = x10 + x9;
		x11 ^= rotl32(u, 7);
		u = x11 + x10;
		x8 ^= rotl32(u, 9);
		u = x8 + x11;
		x9 ^= rotl32(u, 13);
		u = x9 + x8;
		x10 ^= rotl32(u, 18);

		u = x15 + x14;
		x12 ^= rotl32(u, 7);
		u = x12 + x15;
		x13 ^= rotl32(u, 9);
		u = x13 + x12;
		x14 ^= rotl32(u, 13);
		u = x14 + x13;
		x15 ^= rotl32(u, 18);
	}

	x0 += j0;
	x1 += j1;
	x2 += j2;
	x3 += j3;
	x4 += j4;
	x5 += j5;
	x6 += j6;
	x7 += j7;
	x8 += j8;
	x9 += j9;
	x10 += j10;
	x11 += j11;
	x12 += j12;
	x13 += j13;
	x14 += j14;
	x15 += j15;

	block_[0] = (x0 >> 0) & 0xFF; block_[1] = (x0 >> 8) & 0xFF;
	block_[2] = (x0 >> 16) & 0xFF; block_[3] = (x0 >> 24) & 0xFF;
	block_[4] = (x1 >> 0) & 0xFF; block_[5] = (x1 >> 8) & 0xFF;
	block_[6] = (x1 >> 16) & 0xFF; block_[7] = (x1 >> 24) & 0xFF;
	block_[8] = (x2 >> 0) & 0xFF; block_[9] = (x2 >> 8) & 0xFF;
	block_[10] = (x2 >> 16) & 0xFF; block_[11] = (x2 >> 24) & 0xFF;
	block_[12] = (x3 >> 0) & 0xFF; block_[13] = (x3 >> 8) & 0xFF;
	block_[14] = (x3 >> 16) & 0xFF; block_[15] = (x3 >> 24) & 0xFF;
	block_[16] = (x4 >> 0) & 0xFF; block_[17] = (x4 >> 8) & 0xFF;
	block_[18] = (x4 >> 16) & 0xFF; block_[19] = (x4 >> 24) & 0xFF;
	block_[20] = (x5 >> 0) & 0xFF; block_[21] = (x5 >> 8) & 0xFF;
	block_[22] = (x5 >> 16) & 0xFF; block_[23] = (x5 >> 24) & 0xFF;
	block_[24] = (x6 >> 0) & 0xFF; block_[25] = (x6 >> 8) & 0xFF;
	block_[26] = (x6 >> 16) & 0xFF; block_[27] = (x6 >> 24) & 0xFF;
	block_[28] = (x7 >> 0) & 0xFF; block_[29] = (x7 >> 8) & 0xFF;
	block_[30] = (x7 >> 16) & 0xFF; block_[31] = (x7 >> 24) & 0xFF;
	block_[32] = (x8 >> 0) & 0xFF; block_[33] = (x8 >> 8) & 0xFF;
	block_[34] = (x8 >> 16) & 0xFF; block_[35] = (x8 >> 24) & 0xFF;
	block_[36] = (x9 >> 0) & 0xFF; block_[37] = (x9 >> 8) & 0xFF;
	block_[38] = (x9 >> 16) & 0xFF; block_[39] = (x9 >> 24) & 0xFF;
	block_[40] = (x10 >> 0) & 0xFF; block_[41] = (x10 >> 8) & 0xFF;
	block_[42] = (x10 >> 16) & 0xFF; block_[43] = (x10 >> 24) & 0xFF;
	block_[44] = (x11 >> 0) & 0xFF; block_[45] = (x11 >> 8) & 0xFF;
	block_[46] = (x11 >> 16) & 0xFF; block_[47] = (x11 >> 24) & 0xFF;
	block_[48] = (x12 >> 0) & 0xFF; block_[49] = (x12 >> 8) & 0xFF;
	block_[50] = (x12 >> 16) & 0xFF; block_[51] = (x12 >> 24) & 0xFF;
	block_[52] = (x13 >> 0) & 0xFF; block_[53] = (x13 >> 8) & 0xFF;
	block_[54] = (x13 >> 16) & 0xFF; block_[55] = (x13 >> 24) & 0xFF;
	block_[56] = (x14 >> 0) & 0xFF; block_[57] = (x14 >> 8) & 0xFF;
	block_[58] = (x14 >> 16) & 0xFF; block_[59] = (x14 >> 24) & 0xFF;
	block_[60] = (x15 >> 0) & 0xFF; block_[61] = (x15 >> 8) & 0xFF;
	block_[62] = (x15 >> 16) & 0xFF; block_[63] = (x15 >> 24) & 0xFF;
}

} // namespace casc
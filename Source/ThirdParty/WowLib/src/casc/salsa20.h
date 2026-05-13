/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	Based off original works by Dmitry Chestnykh <dmitry@codingrobots.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <string_view>
#include <span>

class BufferWrapper;

namespace casc {

class Salsa20 {
public:
	Salsa20(std::span<const uint8_t> nonce, std::string_view key, int rounds = 20);

	void setKey(std::vector<uint8_t> key);

	void setNonce(std::span<const uint8_t> nonce);

	BufferWrapper getBytes(size_t byteCount);

	BufferWrapper process(BufferWrapper& buf);

private:
	void _reset();

	void _increment();

	void _generateBlock();

	int rounds_;
	std::array<uint32_t, 4> sigma_;
	std::array<uint32_t, 8> keyWords_;
	std::array<uint32_t, 2> nonceWords_;
	std::array<uint32_t, 2> counter_;
	std::array<uint8_t, 64> block_;
	int blockUsed_;
};

} // namespace casc

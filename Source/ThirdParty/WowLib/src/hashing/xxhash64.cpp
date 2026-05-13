/*!
	xxHash64 (C++ uint64_t implementation)
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>

	Original Implementation
	js-xxhash (https://github.com/pierrec/js-xxhash)
	Authors: Pierre Curto (2016)

	License: MIT
*/
#include "xxhash64.h"

#include <cstring>

namespace hashing {

static constexpr uint64_t PRIME64_1 = 11400714785074694791ULL;
static constexpr uint64_t PRIME64_2 = 14029467366897019727ULL;
static constexpr uint64_t PRIME64_3 = 1609587929392839161ULL;
static constexpr uint64_t PRIME64_4 = 9650029242287828579ULL;
static constexpr uint64_t PRIME64_5 = 2870177450012600261ULL;


static inline uint64_t rotl64(uint64_t value, int n) {
	return (value << n) | (value >> (64 - n));
}

static inline uint64_t read_u64_le(const uint8_t* input, std::size_t p) {
	return static_cast<uint64_t>(input[p]) |
		(static_cast<uint64_t>(input[p + 1]) << 8) |
		(static_cast<uint64_t>(input[p + 2]) << 16) |
		(static_cast<uint64_t>(input[p + 3]) << 24) |
		(static_cast<uint64_t>(input[p + 4]) << 32) |
		(static_cast<uint64_t>(input[p + 5]) << 40) |
		(static_cast<uint64_t>(input[p + 6]) << 48) |
		(static_cast<uint64_t>(input[p + 7]) << 56);
}

static inline uint64_t read_u32_le(const uint8_t* input, std::size_t p) {
	return static_cast<uint64_t>(input[p]) |
		(static_cast<uint64_t>(input[p + 1]) << 8) |
		(static_cast<uint64_t>(input[p + 2]) << 16) |
		(static_cast<uint64_t>(input[p + 3]) << 24);
}

XXH64::XXH64(uint64_t seed) {
	init(seed);
}

XXH64& XXH64::init(uint64_t seed) {
	seed_ = seed;
	v1_ = seed + PRIME64_1 + PRIME64_2;
	v2_ = seed + PRIME64_2;
	v3_ = seed;
	v4_ = seed - PRIME64_1;
	total_len_ = 0;
	memsize_ = 0;
	memory_ = {};

	return *this;
}

XXH64& XXH64::update(std::span<const uint8_t> input) {
	std::size_t p = 0;
	const std::size_t len = input.size();
	const std::size_t bEnd = p + len;

	if (len == 0)
		return *this;

	total_len_ += len;

	if (memsize_ + len < 32) {
		std::memcpy(memory_.data() + memsize_, input.data(), len);
		memsize_ += len;
		return *this;
	}

	if (memsize_ > 0) {
		std::memcpy(memory_.data() + memsize_, input.data(), 32 - memsize_);

		std::size_t p64 = 0;
		uint64_t other;

		other = read_u64_le(memory_.data(), p64);
		v1_ += other * PRIME64_2;
		v1_ = rotl64(v1_, 31);
		v1_ *= PRIME64_1;
		p64 += 8;

		other = read_u64_le(memory_.data(), p64);
		v2_ += other * PRIME64_2;
		v2_ = rotl64(v2_, 31);
		v2_ *= PRIME64_1;
		p64 += 8;

		other = read_u64_le(memory_.data(), p64);
		v3_ += other * PRIME64_2;
		v3_ = rotl64(v3_, 31);
		v3_ *= PRIME64_1;
		p64 += 8;

		other = read_u64_le(memory_.data(), p64);
		v4_ += other * PRIME64_2;
		v4_ = rotl64(v4_, 31);
		v4_ *= PRIME64_1;

		p += 32 - memsize_;
		memsize_ = 0;
	}

	if (p + 32 <= bEnd) {
		const std::size_t limit = bEnd - 32;

		do {
			uint64_t other;

			other = read_u64_le(input.data(), p);
			v1_ += other * PRIME64_2;
			v1_ = rotl64(v1_, 31);
			v1_ *= PRIME64_1;
			p += 8;

			other = read_u64_le(input.data(), p);
			v2_ += other * PRIME64_2;
			v2_ = rotl64(v2_, 31);
			v2_ *= PRIME64_1;
			p += 8;

			other = read_u64_le(input.data(), p);
			v3_ += other * PRIME64_2;
			v3_ = rotl64(v3_, 31);
			v3_ *= PRIME64_1;
			p += 8;

			other = read_u64_le(input.data(), p);
			v4_ += other * PRIME64_2;
			v4_ = rotl64(v4_, 31);
			v4_ *= PRIME64_1;
			p += 8;
		} while (p <= limit);
	}

	if (p < bEnd) {
		std::memcpy(memory_.data() + memsize_, input.data() + p, bEnd - p);
		memsize_ = bEnd - p;
	}

	return *this;
}

XXH64& XXH64::update(std::string_view input) {
	return update(std::span<const uint8_t>(
		reinterpret_cast<const uint8_t*>(input.data()), input.size()));
}

uint64_t XXH64::digest() {
	const uint8_t* input = memory_.data();
	std::size_t p = 0;
	const std::size_t bEnd = memsize_;
	uint64_t h64;

	if (total_len_ >= 32) {
		h64 = rotl64(v1_, 1);
		h64 += rotl64(v2_, 7);
		h64 += rotl64(v3_, 12);
		h64 += rotl64(v4_, 18);

		uint64_t v1_temp = v1_ * PRIME64_2;
		v1_temp = rotl64(v1_temp, 31);
		v1_temp *= PRIME64_1;
		h64 ^= v1_temp;
		h64 = h64 * PRIME64_1 + PRIME64_4;

		uint64_t v2_temp = v2_ * PRIME64_2;
		v2_temp = rotl64(v2_temp, 31);
		v2_temp *= PRIME64_1;
		h64 ^= v2_temp;
		h64 = h64 * PRIME64_1 + PRIME64_4;

		uint64_t v3_temp = v3_ * PRIME64_2;
		v3_temp = rotl64(v3_temp, 31);
		v3_temp *= PRIME64_1;
		h64 ^= v3_temp;
		h64 = h64 * PRIME64_1 + PRIME64_4;

		uint64_t v4_temp = v4_ * PRIME64_2;
		v4_temp = rotl64(v4_temp, 31);
		v4_temp *= PRIME64_1;
		h64 ^= v4_temp;
		h64 = h64 * PRIME64_1 + PRIME64_4;
	}
	else {
		h64 = seed_ + PRIME64_5;
	}

	h64 += static_cast<uint64_t>(total_len_);

	while (p + 8 <= bEnd) {
		uint64_t u = read_u64_le(input, p);
		u *= PRIME64_2;
		u = rotl64(u, 31);
		u *= PRIME64_1;
		h64 ^= u;
		h64 = rotl64(h64, 27);
		h64 = h64 * PRIME64_1 + PRIME64_4;
		p += 8;
	}

	if (p + 4 <= bEnd) {
		uint64_t u = read_u32_le(input, p);
		h64 ^= u * PRIME64_1;
		h64 = rotl64(h64, 23);
		h64 = h64 * PRIME64_2 + PRIME64_3;
		p += 4;
	}

	while (p < bEnd) {
		uint64_t u = static_cast<uint64_t>(input[p++]);
		h64 ^= u * PRIME64_5;
		h64 = rotl64(h64, 11);
		h64 *= PRIME64_1;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	init(seed_);

	return h64;
}

uint64_t XXH64::hash(std::span<const uint8_t> input, uint64_t seed) {
	return XXH64(seed).update(input).digest();
}

uint64_t XXH64::hash(std::string_view input, uint64_t seed) {
	return XXH64(seed).update(input).digest();
}

uint64_t xxh64(std::span<const uint8_t> input, uint64_t seed) {
	return XXH64::hash(input, seed);
}

uint64_t xxh64(std::string_view input, uint64_t seed) {
	return XXH64::hash(input, seed);
}

XXH64 xxh64(uint64_t seed) {
	return XXH64(seed);
}

} // namespace hashing

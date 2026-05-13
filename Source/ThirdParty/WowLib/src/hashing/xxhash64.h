/*!
	xxHash64 (C++ uint64_t implementation)
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>

	Original Implementation
	js-xxhash (https://github.com/pierrec/js-xxhash)
	Authors: Pierre Curto (2016)

	License: MIT
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <array>

namespace hashing {

/**
 * XXH64 hash computation class.
 *
 * Supports streaming (multi-call update) and one-shot hashing.
 * JS equivalent: module.exports = XXH64 (constructor + function).
 *
 * In JS, XXH64 acts as both a constructor (new XXH64(seed)) and a
 * direct hash function (XXH64(data, seed)). In C++, use:
 *   - XXH64(seed) constructor for streaming
 *   - XXH64::hash(data, seed) static method for one-shot
 */
class XXH64 {
public:
	/**
	 * Construct a new XXH64 hasher with the given seed.
	 * Equivalent to JS: new XXH64(seed) / new XXH64_state(seed).
	 * @param seed Initialization seed (default 0).
	 */
	explicit XXH64(uint64_t seed = 0);

	/**
	 * Initialize (or reset) the hasher with a new seed.
	 * Equivalent to JS: XXH64_state.prototype.init(seed).
	 * @param seed Initialization seed (default 0).
	 * @return Reference to this for chaining.
	 */
	XXH64& init(uint64_t seed = 0);

	/**
	 * Feed data into the hasher.
	 * Equivalent to JS: XXH64_state.prototype.update(input).
	 * @param input Byte data to hash.
	 * @return Reference to this for chaining.
	 */
	XXH64& update(std::span<const uint8_t> input);

	/**
	 * Feed string data into the hasher (treated as UTF-8 bytes).
	 * Equivalent to the JS update() path that calls toUTF8Array() on strings.
	 * @param input String data to hash.
	 * @return Reference to this for chaining.
	 */
	XXH64& update(std::string_view input);

	/**
	 * Finalize and return the hash value.
	 * Resets the state to the initial seed after computing.
	 * Equivalent to JS: XXH64_state.prototype.digest().
	 * @return 64-bit hash value.
	 */
	uint64_t digest();

	/**
	 * One-shot hash computation.
	 * Equivalent to JS: XXH64(input_data, seed) called as a function.
	 * @param input Byte data to hash.
	 * @param seed Initialization seed (default 0).
	 * @return 64-bit hash value.
	 */
	static uint64_t hash(std::span<const uint8_t> input, uint64_t seed = 0);

	/**
	 * One-shot hash computation for strings.
	 * Equivalent to JS: XXH64(string_data, seed) called as a function.
	 * @param input String data to hash (treated as UTF-8 bytes).
	 * @param seed Initialization seed (default 0).
	 * @return 64-bit hash value.
	 */
	static uint64_t hash(std::string_view input, uint64_t seed = 0);

private:
	uint64_t seed_;
	uint64_t v1_;
	uint64_t v2_;
	uint64_t v3_;
	uint64_t v4_;
	uint64_t total_len_;
	std::size_t memsize_;
	std::array<uint8_t, 32> memory_;
};

/**
 * JS-style callable helpers mirroring module.exports = XXH64 function behavior.
 * - xxh64(input, seed?) => one-shot hash
 * - xxh64(seed) => stateful hasher (constructor-like)
 */
uint64_t xxh64(std::span<const uint8_t> input, uint64_t seed = 0);
uint64_t xxh64(std::string_view input, uint64_t seed = 0);
XXH64 xxh64(uint64_t seed);

} // namespace hashing

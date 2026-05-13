/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <future>
#include <unordered_map>
#include <deque>
#include <vector>
#include <optional>

#include "../buffer.h"
#include "../blob.h"
#include "blte-reader.h"

namespace casc {

/**
 * Streaming BLTE reader that fetches and decodes blocks on demand.
 *
 * JS equivalent: class BLTEStreamReader — module.exports = BLTEStreamReader
 *
 * C++ exposes synchronous methods used by existing call sites plus
 * Promise-style async-equivalent wrappers (*Async) for JS API parity.
 */
class BLTEStreamReader {
public:
	using BlockFetcher = std::function<BufferWrapper(size_t)>;
	using BlockFetcherAsync = std::function<std::future<BufferWrapper>(size_t)>;

	class ReadableStream {
	public:
		explicit ReadableStream(BLTEStreamReader* owner);
		std::optional<std::vector<uint8_t>> pull();
		void cancel();

	private:
		BLTEStreamReader* owner = nullptr;
		size_t currentBlock = 0;
	};

	/**
	 * Construct a new BLTEStreamReader instance.
	 * @param hash Expected MD5 hash.
	 * @param metadata BLTE metadata with blocks, headerSize, dataStart, totalSize.
	 * @param blockFetcher Function that fetches raw block data by index.
	 * @param partialDecrypt If true, allow partial decryption (leave zeroed data).
	 */
	BLTEStreamReader(const std::string& hash, const BLTEMetadata& metadata,
		BlockFetcher blockFetcher, bool partialDecrypt = false);
	BLTEStreamReader(const std::string& hash, const BLTEMetadata& metadata,
		BlockFetcherAsync blockFetcherAsync, bool partialDecrypt = false);

	/**
	 * Fetch and decode a single block on demand.
	 * @param blockIndex Index of the block to fetch.
	 * @returns Decoded block data.
	 */
	BufferWrapper getBlock(size_t blockIndex);
	std::future<BufferWrapper> getBlockAsync(size_t blockIndex);

	/** C++ equivalent of JS createReadableStream() progressive pull/cancel API. */
	ReadableStream createReadableStream();

	/**
	 * Iterate all blocks, invoking callback for each decoded block.
	 * @param callback Function called with each decoded block.
	 */
	void streamBlocks(const std::function<void(BufferWrapper&)>& callback);
	std::future<void> streamBlocksAsync(const std::function<void(BufferWrapper&)>& callback);

	/**
	 * Concatenate all decoded blocks into a single BufferWrapper.
	 * @returns BufferWrapper containing all decoded block data.
	 */
	std::string createBlobURL();
	std::future<std::string> createBlobURLAsync();

	/**
	 * Get total decompressed size.
	 * @returns Total decompressed size in bytes.
	 */
	size_t getTotalSize() const;

	/**
	 * Get number of blocks.
	 * @returns Number of BLTE blocks.
	 */
	size_t getBlockCount() const;

	/**
	 * Clear block cache.
	 */
	void clearCache();

private:
	/**
	 * Decode a BLTE block based on its type flag.
	 * @param blockData Raw block data.
	 * @param index Block index.
	 * @returns Decoded block data.
	 */
	BufferWrapper _decodeBlock(BufferWrapper blockData, size_t index);
	std::future<BufferWrapper> _decodeBlockAsync(BufferWrapper blockData, size_t index);

	/**
	 * Decrypt an encrypted BLTE block.
	 * @param data Buffer positioned after the encryption flag byte.
	 * @param index Block index.
	 * @returns Decrypted block data.
	 */
	BufferWrapper _decryptBlock(BufferWrapper& data, size_t index);

	std::string hash;
	BLTEMetadata metadata;
	BlockFetcher blockFetcher;
	BlockFetcherAsync blockFetcherAsync;
	bool partialDecrypt;

	// FIFO block cache: unordered_map for O(1) lookup, deque preserves JS `Map` insertion order
	// (JS evicts via `blockCache.keys().next().value`, which yields the oldest insertion).
	std::unordered_map<size_t, BufferWrapper> blockCache;
	std::deque<size_t> cacheOrder;
	size_t maxCacheSize = 10;
};

} // namespace casc

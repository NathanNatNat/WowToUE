/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "blte-stream-reader.h"
#include "salsa20.h"
#include "tact-keys.h"
#include "../log.h"

#include <format>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <future>

namespace casc {

BLTEStreamReader::BLTEStreamReader(const std::string& hash, const BLTEMetadata& metadata,
	BlockFetcher blockFetcher, bool partialDecrypt)
	: hash(hash), metadata(metadata), blockFetcher(std::move(blockFetcher)),
	  partialDecrypt(partialDecrypt) {
	this->blockFetcherAsync = [fetcher = this->blockFetcher](size_t blockIndex) mutable {
		return std::async(std::launch::deferred, [fetcher, blockIndex]() mutable {
			return fetcher(blockIndex);
		});
	};
}

BLTEStreamReader::BLTEStreamReader(const std::string& hash, const BLTEMetadata& metadata,
	BlockFetcherAsync blockFetcherAsync, bool partialDecrypt)
	: hash(hash), metadata(metadata),
	  blockFetcherAsync(std::move(blockFetcherAsync)),
	  partialDecrypt(partialDecrypt) {
	this->blockFetcher = [this](size_t blockIndex) {
		return this->blockFetcherAsync(blockIndex).get();
	};
}

BufferWrapper BLTEStreamReader::getBlock(size_t blockIndex) {
	return getBlockAsync(blockIndex).get();
}

std::future<BufferWrapper> BLTEStreamReader::getBlockAsync(size_t blockIndex) {
	return std::async(std::launch::deferred, [this, blockIndex]() {
		if (blockCache.contains(blockIndex))
			return blockCache.at(blockIndex);

		if (blockIndex >= metadata.blocks.size())
			throw std::out_of_range(std::format("BLTE block index out of range: {}", blockIndex));

		const BLTEBlock& blockMeta = metadata.blocks[blockIndex];
		BufferWrapper rawBlock = blockFetcherAsync(blockIndex).get();

		// Verify block hash if not empty.
		if (blockMeta.Hash != EMPTY_HASH) {
			const std::string blockHash = rawBlock.calculateHash();
			if (blockHash != blockMeta.Hash)
				throw BLTEIntegrityError(blockMeta.Hash, blockHash);
		}

		BufferWrapper decoded = _decodeBlockAsync(std::move(rawBlock), blockIndex).get();

		blockCache.emplace(blockIndex, decoded);
		cacheOrder.push_back(blockIndex);

		if (blockCache.size() > maxCacheSize) {
			const size_t firstKey = cacheOrder.front();
			cacheOrder.pop_front();
			blockCache.erase(firstKey);
		}

		return decoded;
	});
}

BufferWrapper BLTEStreamReader::_decodeBlock(BufferWrapper blockData, size_t index) {
	return _decodeBlockAsync(std::move(blockData), index).get();
}

std::future<BufferWrapper> BLTEStreamReader::_decodeBlockAsync(BufferWrapper blockData, size_t index) {
	return std::async(std::launch::deferred, [this, blockData = std::move(blockData), index]() mutable {
		const uint8_t flag = blockData.readUInt8();

		switch (flag) {
			case 0x45: { // Encrypted.
				try {
					BufferWrapper decrypted = _decryptBlock(blockData, index);
					return _decodeBlockAsync(std::move(decrypted), index).get();
				} catch (const EncryptionError&) {
					if (partialDecrypt) {
						// Return zeroed buffer.
						const int32_t size = metadata.blocks[index].DecompSize;
						return BufferWrapper::alloc(static_cast<size_t>(size), true);
					}
					throw;
				}
			}

			case 0x4e: // Normal (uncompressed).
				return blockData.readBuffer(blockData.remainingBytes());

			case 0x5a: // Compressed.
				return std::get<BufferWrapper>(blockData.readBuffer(blockData.remainingBytes(), true, true));

			case 0x46: // Frame (recursive).
				throw std::runtime_error("[BLTE] No frame decoder implemented!");

			default:
				throw std::runtime_error(std::format("Unknown BLTE block type: {}", flag));
		}
	});
}

BufferWrapper BLTEStreamReader::_decryptBlock(BufferWrapper& data, size_t index) {
	const uint8_t keyNameSize = data.readUInt8();
	if (keyNameSize == 0 || keyNameSize != 8)
		throw std::runtime_error(std::format("[BLTE] Unexpected keyNameSize: {}", keyNameSize));

	std::vector<std::string> keyNameBytes(keyNameSize);
	for (uint8_t i = 0; i < keyNameSize; i++)
		keyNameBytes[i] = data.readHexString(1);

	std::string keyName;
	for (auto it = keyNameBytes.rbegin(); it != keyNameBytes.rend(); ++it)
		keyName += *it;

	const uint8_t ivSize = data.readUInt8();

	if ((ivSize != 4 && ivSize != 8) || ivSize > 8)
		throw std::runtime_error(std::format("[BLTE] Unexpected ivSize: {}", ivSize));

	std::vector<uint8_t> ivShort = data.readUInt8(ivSize);
	if (data.remainingBytes() == 0)
		throw std::runtime_error("[BLTE] Unexpected end of data before encryption flag.");

	const uint8_t encryptType = data.readUInt8();
	if (encryptType != ENC_TYPE_SALSA20)
		throw std::runtime_error(std::format("[BLTE] Unexpected encryption type: {}", encryptType));

	for (int shift = 0, i = 0; i < 4; shift += 8, i++)
		ivShort[i] = (ivShort[i] ^ ((index >> shift) & 0xFF)) & 0xFF;

	const auto key = tact_keys::getKey(keyName);
	if (!key.has_value())
		throw EncryptionError(keyName);

	std::vector<uint8_t> nonce(8, 0);
	for (size_t i = 0; i < 8; i++)
		nonce[i] = (i < ivShort.size() ? ivShort[i] : 0x0);

	Salsa20 instance(std::span<const uint8_t>(nonce), *key);
	BufferWrapper encData = data.readBuffer(data.remainingBytes());
	return instance.process(encData);
}

BLTEStreamReader::ReadableStream BLTEStreamReader::createReadableStream() {
	return ReadableStream(this);
}

void BLTEStreamReader::streamBlocks(const std::function<void(BufferWrapper&)>& callback) {
	streamBlocksAsync(callback).get();
}

std::future<void> BLTEStreamReader::streamBlocksAsync(const std::function<void(BufferWrapper&)>& callback) {
	return std::async(std::launch::deferred, [this, callback]() {
		for (size_t i = 0; i < metadata.blocks.size(); i++) {
			BufferWrapper block = getBlockAsync(i).get();
			callback(block);
		}
	});
}

std::string BLTEStreamReader::createBlobURL() {
	return createBlobURLAsync().get();
}

std::future<std::string> BLTEStreamReader::createBlobURLAsync() {
	return std::async(std::launch::deferred, [this]() {
		std::vector<BlobPart> chunks;

		for (size_t i = 0; i < metadata.blocks.size(); i++) {
			BufferWrapper block = getBlockAsync(i).get();
			chunks.emplace_back(block.raw());
		}

		const BlobPolyfill blob(std::move(chunks), BlobOptions{.type = "video/x-msvideo"});
		return URLPolyfill::createObjectURL(blob);
	});
}

size_t BLTEStreamReader::getTotalSize() const {
	return metadata.totalSize;
}

size_t BLTEStreamReader::getBlockCount() const {
	return metadata.blocks.size();
}

void BLTEStreamReader::clearCache() {
	blockCache.clear();
	cacheOrder.clear();
}

BLTEStreamReader::ReadableStream::ReadableStream(BLTEStreamReader* owner)
	: owner(owner) {}

std::optional<std::vector<uint8_t>> BLTEStreamReader::ReadableStream::pull() {
	if (owner == nullptr || currentBlock >= owner->metadata.blocks.size())
		return std::nullopt;

	BufferWrapper decodedBlock = owner->getBlockAsync(currentBlock).get();
	currentBlock++;
	return decodedBlock.toVector();
}

void BLTEStreamReader::ReadableStream::cancel() {
	if (owner != nullptr)
		owner->clearCache();
}

} // namespace casc

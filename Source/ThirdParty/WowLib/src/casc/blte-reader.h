/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>

#include "../buffer.h"

namespace casc {

constexpr uint32_t BLTE_MAGIC = 0x45544c42;
constexpr uint8_t ENC_TYPE_SALSA20 = 0x53;
inline const std::string EMPTY_HASH = "00000000000000000000000000000000";

class EncryptionError : public std::runtime_error {
public:
	EncryptionError(const std::string& key);
	std::string key;
};

class BLTEIntegrityError : public std::runtime_error {
public:
	BLTEIntegrityError(const std::string& expected, const std::string& actual);
};

struct BLTEBlock {
	int32_t CompSize;
	int32_t DecompSize;
	std::string Hash;
	int64_t fileOffset;
};

struct BLTEMetadata {
	std::vector<BLTEBlock> blocks;
	int32_t headerSize;
	size_t dataStart;
	size_t totalSize;
};

class BLTEReader : public BufferWrapper {
public:
	/**
	 * Check if the given data is a BLTE file.
	 * @param data BufferWrapper to check.
	 */
	static bool checkBLTE(BufferWrapper& data);

	/**
	 * Parse BLTE header and return metadata without allocating full buffer.
	 * @param buf Raw BLTE data buffer.
	 * @param hash Expected MD5 hash.
	 * @param verifyHash If true, verify hash against header.
	 * @param restoreOffset If true, restore buffer offset after parsing.
	 * @returns BLTEMetadata with blocks, headerSize, dataStart, totalSize.
	 */
	static BLTEMetadata parseBLTEHeader(BufferWrapper& buf, const std::string& hash, bool verifyHash = true, bool restoreOffset = true);

	/**
	 * Construct a new BLTEReader instance.
	 * @param buf Raw BLTE data buffer.
	 * @param hash Expected MD5 hash.
	 * @param partialDecrypt If true, allow partial decryption (leave zeroed data).
	 */
	BLTEReader(BufferWrapper buf, const std::string& hash, bool partialDecrypt = false);

	/**
	 * Process all BLTE blocks in the reader.
	 */
	void processAllBlocks();

	/**
	 * Write the contents of this buffer to a file.
	 * Directory path will be created if needed.
	 * @param file Output file path.
	 */
	void writeToFile(const std::filesystem::path& file);

	/**
	 * Decode this buffer using the given audio context.
	 * Context parameter is preserved for JS API parity.
	 */
	BufferWrapper::DecodedAudioData decodeAudio(void* context = nullptr);

	/**
	 * Assign a data URL for this buffer.
	 * @returns Data URL string.
	 */
	const std::string& getDataURL();

protected:
	/**
	 * Override _checkBounds to lazily decompress BLTE blocks on demand.
	 * Matches the original JS behaviour where reads automatically trigger
	 * block decompression as needed.
	 * @param length Number of bytes required.
	 */
	void _checkBounds(size_t length) override;

private:
	/**
	 * Process the next BLTE block.
	 * @returns false if no more blocks, true otherwise.
	 */
	bool _processBlock();

	/**
	 * Handle a BLTE block.
	 * @param block Buffer positioned at block data.
	 * @param blockEnd End offset of block in the buffer.
	 * @param index Block index.
	 */
	void _handleBlock(BufferWrapper& block, size_t blockEnd, size_t index);

	/**
	 * Decompress BLTE block.
	 * @param data Buffer with compressed data.
	 * @param blockEnd End offset of block.
	 * @param index Block index.
	 */
	void _decompressBlock(BufferWrapper& data, size_t blockEnd, size_t index);

	/**
	 * Decrypt BLTE block.
	 * @param data Buffer with encrypted data.
	 * @param blockEnd End offset of block.
	 * @param index Block index.
	 * @returns Decrypted buffer.
	 */
	BufferWrapper _decryptBlock(BufferWrapper& data, size_t blockEnd, size_t index);

	/**
	 * Write the contents of a buffer to this instance.
	 * Skips bound checking for BLTE internal writing.
	 * @param buf Source buffer.
	 * @param blockEnd End offset in the source buffer.
	 */
	void _writeBufferBLTE(BufferWrapper& buf, size_t blockEnd);

	BufferWrapper _blte;
	std::vector<BLTEBlock> blocks;
	size_t blockIndex = 0;
	size_t blockWriteIndex = 0;
	bool partialDecrypt = false;
};

} // namespace casc

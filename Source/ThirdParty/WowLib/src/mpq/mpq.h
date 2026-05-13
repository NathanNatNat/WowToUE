/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <optional>
#include <array>
#include <span>

namespace mpq {

static constexpr uint32_t MPQ_FILE_IMPLODE       = 0x00000100;
static constexpr uint32_t MPQ_FILE_COMPRESS       = 0x00000200;
static constexpr uint32_t MPQ_FILE_ENCRYPTED      = 0x00010000;
static constexpr uint32_t MPQ_FILE_FIX_KEY        = 0x00020000;
static constexpr uint32_t MPQ_FILE_SINGLE_UNIT    = 0x01000000;
static constexpr uint32_t MPQ_FILE_DELETE_MARKER  = 0x02000000;
static constexpr uint32_t MPQ_FILE_SECTOR_CRC     = 0x04000000;
static constexpr uint32_t MPQ_FILE_EXISTS         = 0x80000000;

static constexpr uint32_t HASH_ENTRY_EMPTY   = 0xFFFFFFFF;
static constexpr uint32_t HASH_ENTRY_DELETED = 0xFFFFFFFE;

enum HashType : int {
	TABLE_OFFSET = 0,
	HASH_A = 1,
	HASH_B = 2,
	TABLE = 3,
};

struct UserDataHeader {
	std::string magic;
	uint32_t userDataSize;
	uint32_t mpqHeaderOffset;
	uint32_t userDataHeaderSize;
	std::vector<uint8_t> content;
};

struct MPQHeader {
	std::string magic;
	uint32_t headerSize;
	uint32_t archivedSize;
	uint16_t formatVersion;
	uint16_t sectorSizeShift;
	uint32_t hashTableOffset;
	uint32_t blockTableOffset;
	uint32_t hashTableEntries;
	uint32_t blockTableEntries;
	uint32_t offset;
	std::optional<UserDataHeader> userDataHeader;

	// Format v1 extended fields
	uint64_t extendedBlockTableOffset = 0;
	int16_t hashTableOffsetHigh = 0;
	int16_t blockTableOffsetHigh = 0;
};

struct HashTableEntry {
	uint32_t hashA;
	uint32_t hashB;
	uint16_t locale;
	uint16_t platform;
	uint32_t blockTableIndex;
};

struct BlockTableEntry {
	uint32_t offset;
	uint32_t archivedSize;
	uint32_t size;
	uint32_t flags;
};

struct ArchiveInfo {
	uint16_t formatVersion;
	uint32_t archiveSize;
	size_t fileCount;
	uint32_t hashTableEntries;
	uint32_t blockTableEntries;
};

/**
 * MPQArchive — reads and extracts files from MPQ archives.
 *
 * JS equivalent: class MPQArchive — module.exports = { MPQArchive }
 */
class MPQArchive {
public:
	explicit MPQArchive(const std::string& filePath);
	~MPQArchive();

	MPQArchive(const MPQArchive&) = delete;
	MPQArchive& operator=(const MPQArchive&) = delete;
	MPQArchive(MPQArchive&& other) noexcept;
	MPQArchive& operator=(MPQArchive&& other) noexcept;

	void close();

	/**
	 * Extract a file by its internal path name.
	 * @param filename The internal file path (e.g. "(listfile)").
	 * @return File data, or empty optional if not found.
	 */
	std::optional<std::vector<uint8_t>> extractFile(const std::string& filename);

	/**
	 * Extract a file by its block table index.
	 * @param blockIndex Index into the block table.
	 * @return File data, or empty optional if not found.
	 */
	std::optional<std::vector<uint8_t>> extractFileByBlockIndex(uint32_t blockIndex);

	/**
	 * Get archive metadata.
	 */
	ArchiveInfo getInfo() const;

	/**
	 * Get hash table entry for a filename.
	 * @param filename The internal file path.
	 * @return Pointer to the hash entry, or nullptr if not found.
	 */
	const HashTableEntry* getHashTableEntry(const std::string& filename) const;

	/**
	 * Get valid (non-empty, non-deleted) hash entries.
	 */
	std::vector<HashTableEntry> getValidHashEntries() const;

	/**
	 * Get valid block entries with their indices.
	 */
	std::vector<std::pair<uint32_t, BlockTableEntry>> getValidBlockEntries() const;

	/// List of files from the internal (listfile).
	std::vector<std::string> files;

	/// Block table entries (public for MPQInstall access).
	std::vector<BlockTableEntry> blockTable;

private:
	std::vector<uint8_t> readBytes(uint64_t offset, size_t length);
	std::array<uint32_t, 0x500> buildEncryptionTable();
	MPQHeader readHeader();
	UserDataHeader readUserDataHeader(uint64_t offset);
	MPQHeader readMPQHeader(uint64_t offset);
	uint32_t hash(const std::string& str, HashType hashType) const;
	std::vector<uint8_t> decrypt(std::span<const uint8_t> data, uint32_t key) const;
	uint32_t detectFileSeed(std::span<const uint8_t> data, uint32_t expected) const;
	std::vector<HashTableEntry> readHashTable();
	std::vector<BlockTableEntry> readBlockTable();
	std::vector<uint8_t> decompress(std::span<const uint8_t> data, size_t expected_size);
	std::vector<uint8_t> inflateData(std::span<const uint8_t> data);

	std::string filePath;
	int fd;
	std::array<uint32_t, 0x500> encryptionTable;
	MPQHeader header;
	std::vector<HashTableEntry> hashTable;
};

} // namespace mpq

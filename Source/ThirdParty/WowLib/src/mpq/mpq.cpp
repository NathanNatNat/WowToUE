/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "mpq.h"
#include "pkware.h"
#include "huffman.h"
#include "bzip2.h"

#include <zlib.h>

#include <cstring>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <format>
#include <numeric>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <share.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif

namespace mpq {

namespace {

int openFileReadOnly(const std::string& path) {
#ifdef _WIN32
	int fd = -1;
	_sopen_s(&fd, path.c_str(), _O_RDONLY | _O_BINARY, _SH_DENYNO, 0);
	return fd;
#else
	return ::open(path.c_str(), O_RDONLY);
#endif
}

void closeFile(int fd) {
	if (fd >= 0) {
#ifdef _WIN32
		_close(fd);
#else
		::close(fd);
#endif
	}
}

void readFile(int fd, void* buffer, size_t length, int64_t offset) {
#ifdef _WIN32
	_lseeki64(fd, offset, SEEK_SET);
	_read(fd, buffer, static_cast<unsigned int>(length));
#else
	::lseek(fd, static_cast<off_t>(offset), SEEK_SET);
	::read(fd, buffer, length);
#endif
}

static inline uint32_t readU32LE(const uint8_t* p) {
	return static_cast<uint32_t>(p[0]) |
	       (static_cast<uint32_t>(p[1]) << 8) |
	       (static_cast<uint32_t>(p[2]) << 16) |
	       (static_cast<uint32_t>(p[3]) << 24);
}

static inline uint16_t readU16LE(const uint8_t* p) {
	return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static inline int16_t readI16LE(const uint8_t* p) {
	return static_cast<int16_t>(readU16LE(p));
}

static inline void writeU32LE(uint8_t* p, uint32_t v) {
	p[0] = static_cast<uint8_t>(v);
	p[1] = static_cast<uint8_t>(v >> 8);
	p[2] = static_cast<uint8_t>(v >> 16);
	p[3] = static_cast<uint8_t>(v >> 24);
}

} // anonymous namespace

MPQArchive::MPQArchive(const std::string& filePath)
	: filePath(filePath), fd(-1), encryptionTable{}, header{} {

	fd = openFileReadOnly(filePath);
	if (fd < 0)
		throw std::runtime_error(std::format("failed to open MPQ file: {}", filePath));

	encryptionTable = buildEncryptionTable();
	header = readHeader();
	hashTable = readHashTable();
	blockTable = readBlockTable();

	try {
		auto listfile_buffer = extractFile("(listfile)");
		if (listfile_buffer && !listfile_buffer->empty()) {
			std::string listfile_text(listfile_buffer->begin(), listfile_buffer->end());

			size_t start = 0;
			while (start < listfile_text.size()) {
				size_t end = listfile_text.find_first_of("\r\n", start);
				if (end == std::string::npos)
					end = listfile_text.size();

				if (end > start) {
					std::string line = listfile_text.substr(start, end - start);
					if (!line.empty())
						files.push_back(std::move(line));
				}

				start = end;
				while (start < listfile_text.size() && (listfile_text[start] == '\r' || listfile_text[start] == '\n'))
					start++;
			}
		}
	} catch (...) {
		// no listfile
	}
}

MPQArchive::~MPQArchive() {
	close();
}

MPQArchive::MPQArchive(MPQArchive&& other) noexcept
	: filePath(std::move(other.filePath)), fd(other.fd),
	  encryptionTable(other.encryptionTable), header(std::move(other.header)),
	  hashTable(std::move(other.hashTable)), blockTable(std::move(other.blockTable)),
	  files(std::move(other.files)) {
	other.fd = -1;
}

MPQArchive& MPQArchive::operator=(MPQArchive&& other) noexcept {
	if (this != &other) {
		close();
		filePath = std::move(other.filePath);
		fd = other.fd;
		encryptionTable = other.encryptionTable;
		header = std::move(other.header);
		hashTable = std::move(other.hashTable);
		blockTable = std::move(other.blockTable);
		files = std::move(other.files);
		other.fd = -1;
	}
	return *this;
}

void MPQArchive::close() {
	if (fd >= 0) {
		closeFile(fd);
		fd = -1;
	}
}

std::vector<uint8_t> MPQArchive::readBytes(uint64_t offset, size_t length) {
	std::vector<uint8_t> buffer(length);
	readFile(fd, buffer.data(), length, static_cast<int64_t>(offset));
	return buffer;
}

std::array<uint32_t, 0x500> MPQArchive::buildEncryptionTable() {
	std::array<uint32_t, 0x500> table{};
	uint32_t seed = 0x00100001;

	for (uint32_t i = 0; i < 0x100; i++) {
		uint32_t index = i;
		for (uint32_t j = 0; j < 5; j++) {
			seed = (seed * 125 + 3) % 0x2aaaab;
			const uint32_t temp1 = (seed & 0xffff) << 0x10;

			seed = (seed * 125 + 3) % 0x2aaaab;
			const uint32_t temp2 = seed & 0xffff;

			table[index] = temp1 | temp2;
			index += 0x100;
		}
	}

	return table;
}

MPQHeader MPQArchive::readHeader() {
	const std::array<uint64_t, 8> possible_offsets = {{ 0, 0x200, 0x400, 0x600, 0x800, 0xa00, 0xc00, 0xe00 }};
	int64_t found_offset = -1;
	uint64_t offset = 0;

	for (const auto test_offset : possible_offsets) {
		auto data = readBytes(test_offset, 4);

		std::string magic(data.begin(), data.end());

		if (magic == "MPQ\x1a" || magic == "MPQ\x1b") {
			found_offset = static_cast<int64_t>(test_offset);
			offset = test_offset;
			break;
		}
	}

	if (found_offset == -1)
		throw std::runtime_error("invalid MPQ archive: MPQ signature not found");

	auto data = readBytes(offset, 4);
	std::string magic(data.begin(), data.end());

	MPQHeader mpq_header{};

	if (magic == "MPQ\x1b") {
		auto user_data_header = readUserDataHeader(offset);
		offset = user_data_header.mpqHeaderOffset;
		mpq_header = readMPQHeader(offset);
		mpq_header.userDataHeader = std::move(user_data_header);
	} else if (magic == "MPQ\x1a") {
		mpq_header = readMPQHeader(offset);
	} else {
		throw std::runtime_error("invalid MPQ archive: invalid magic signature");
	}

	return mpq_header;
}

UserDataHeader MPQArchive::readUserDataHeader(uint64_t offset) {
	auto data = readBytes(offset, 16);

	UserDataHeader udh;
	udh.magic = std::string(data.begin(), data.begin() + 4);
	udh.userDataSize = readU32LE(data.data() + 4);
	udh.mpqHeaderOffset = readU32LE(data.data() + 8);
	udh.userDataHeaderSize = readU32LE(data.data() + 12);

	udh.content = readBytes(offset + 16, udh.userDataHeaderSize);

	return udh;
}

MPQHeader MPQArchive::readMPQHeader(uint64_t offset) {
	auto data = readBytes(offset, 44);

	MPQHeader h;
	h.magic = std::string(data.begin(), data.begin() + 4);
	h.headerSize = readU32LE(data.data() + 4);
	h.archivedSize = readU32LE(data.data() + 8);
	h.formatVersion = readU16LE(data.data() + 12);
	h.sectorSizeShift = readU16LE(data.data() + 14);
	h.hashTableOffset = readU32LE(data.data() + 16);
	h.blockTableOffset = readU32LE(data.data() + 20);
	h.hashTableEntries = readU32LE(data.data() + 24);
	h.blockTableEntries = readU32LE(data.data() + 28);
	h.offset = static_cast<uint32_t>(offset);

	if (h.formatVersion == 1) {
		const uint32_t ext_block_table_low = readU32LE(data.data() + 32);
		const uint32_t ext_block_table_high = readU32LE(data.data() + 36);

		h.extendedBlockTableOffset = (static_cast<uint64_t>(ext_block_table_high) << 32) | static_cast<uint64_t>(ext_block_table_low);
		h.hashTableOffsetHigh = readI16LE(data.data() + 40);
		h.blockTableOffsetHigh = readI16LE(data.data() + 42);
	}

	return h;
}

uint32_t MPQArchive::hash(const std::string& str, HashType hashType) const {
	uint32_t seed1 = 0x7fed7fed;
	uint32_t seed2 = 0xeeeeeeee;

	std::string upper = str;
	std::transform(upper.begin(), upper.end(), upper.begin(),
	               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

	for (size_t i = 0; i < upper.size(); i++) {
		const uint32_t ch = static_cast<uint8_t>(upper[i]);
		const uint32_t value = encryptionTable[(static_cast<uint32_t>(hashType) << 8) + ch];
		seed1 = value ^ (seed1 + seed2);
		seed2 = ch + seed1 + seed2 + (seed2 << 5) + 3;
	}

	return seed1;
}

std::vector<uint8_t> MPQArchive::decrypt(std::span<const uint8_t> data, uint32_t key) const {
	uint32_t seed1 = key;
	uint32_t seed2 = 0xeeeeeeee;

	std::vector<uint8_t> result(data.size());

	for (size_t i = 0; i < data.size() / 4; i++) {
		const uint32_t temp = seed2 + encryptionTable[0x400 + (seed1 & 0xff)];
		seed2 = temp;

		const uint32_t value = readU32LE(data.data() + i * 4);
		const uint32_t decrypted = value ^ (seed1 + seed2);

		seed1 = ((~seed1 << 21) + 0x11111111) | (seed1 >> 11);
		seed2 = decrypted + seed2 + (seed2 << 5) + 3;

		writeU32LE(result.data() + i * 4, decrypted);
	}

	return result;
}

uint32_t MPQArchive::detectFileSeed(std::span<const uint8_t> data, uint32_t expected) const {
	if (data.size() < 8)
		return 0;

	const uint32_t encrypted0 = readU32LE(data.data());
	const uint32_t encrypted1 = readU32LE(data.data() + 4);

	const uint32_t seed_sum = encrypted0 ^ expected;
	const uint32_t temp = seed_sum - 0xEEEEEEEE;

	for (uint32_t low_byte = 0; low_byte < 256; low_byte++) {
		const uint32_t seed1 = temp - encryptionTable[0x400 + low_byte];

		const uint32_t seed2_0 = 0xEEEEEEEE + encryptionTable[0x400 + (seed1 & 0xFF)];
		const uint32_t decrypted0 = encrypted0 ^ (seed1 + seed2_0);

		if (decrypted0 != expected)
			continue;

		const uint32_t next_seed1 = ((~seed1 << 21) + 0x11111111) | (seed1 >> 11);
		const uint32_t seed2_1 = decrypted0 + seed2_0 + (seed2_0 << 5) + 3 + encryptionTable[0x400 + (next_seed1 & 0xFF)];
		const uint32_t decrypted1 = encrypted1 ^ (next_seed1 + seed2_1);

		if ((decrypted1 & 0xFFFF0000) == 0)
			return seed1;
	}

	return 0;
}

std::vector<HashTableEntry> MPQArchive::readHashTable() {
	const uint64_t table_offset = static_cast<uint64_t>(header.hashTableOffset) + header.offset;
	const size_t table_size = static_cast<size_t>(header.hashTableEntries) * 16;
	const uint32_t key = hash("(hash table)", HashType::TABLE);

	auto table_data = readBytes(table_offset, table_size);
	auto decrypted = decrypt(table_data, key);
	table_data = std::move(decrypted);

	std::vector<HashTableEntry> entries;
	entries.reserve(header.hashTableEntries);

	for (uint32_t i = 0; i < header.hashTableEntries; i++) {
		const size_t offset = static_cast<size_t>(i) * 16;
		entries.push_back({
			readU32LE(table_data.data() + offset),
			readU32LE(table_data.data() + offset + 4),
			readU16LE(table_data.data() + offset + 8),
			readU16LE(table_data.data() + offset + 10),
			readU32LE(table_data.data() + offset + 12),
		});
	}

	return entries;
}

std::vector<BlockTableEntry> MPQArchive::readBlockTable() {
	const uint64_t table_offset = static_cast<uint64_t>(header.blockTableOffset) + header.offset;
	const size_t table_size = static_cast<size_t>(header.blockTableEntries) * 16;
	const uint32_t key = hash("(block table)", HashType::TABLE);

	auto table_data = readBytes(table_offset, table_size);
	auto decrypted = decrypt(table_data, key);
	table_data = std::move(decrypted);

	std::vector<BlockTableEntry> entries;
	entries.reserve(header.blockTableEntries);

	for (uint32_t i = 0; i < header.blockTableEntries; i++) {
		const size_t offset = static_cast<size_t>(i) * 16;
		entries.push_back({
			readU32LE(table_data.data() + offset),
			readU32LE(table_data.data() + offset + 4),
			readU32LE(table_data.data() + offset + 8),
			readU32LE(table_data.data() + offset + 12),
		});
	}

	return entries;
}

const HashTableEntry* MPQArchive::getHashTableEntry(const std::string& filename) const {
	const uint32_t hash_table_index = hash(filename, HashType::TABLE_OFFSET) & (header.hashTableEntries - 1);
	const uint32_t hash_a = hash(filename, HashType::HASH_A);
	const uint32_t hash_b = hash(filename, HashType::HASH_B);

	for (size_t i = hash_table_index; i < hashTable.size(); i++) {
		const auto& entry = hashTable[i];
		if (entry.hashA == hash_a && entry.hashB == hash_b) {
			if (entry.blockTableIndex == 0xFFFFFFFF || entry.blockTableIndex == 0xFFFFFFFE)
				return nullptr; // empty or deleted
			return &entry;
		}

		if (entry.hashA == 0xFFFFFFFF && entry.hashB == 0xFFFFFFFF)
			break; // file doesn't exist
	}

	return nullptr;
}

std::vector<uint8_t> MPQArchive::decompress(std::span<const uint8_t> data, size_t expected_size) {
	// 0x01: Huffman
	// 0x02: Zlib
	// 0x08: PKWare
	// 0x10: Bzip2
	// 0x40: ADPCM Mono
	// 0x80: ADPCM Stereo

	if (data.empty())
		return std::vector<uint8_t>(data.begin(), data.end());

	uint8_t compression_flags = data[0];
	std::vector<uint8_t> result(data.begin() + 1, data.end());

	if (compression_flags == 0)
		return result;

	// order: Bzip2 (0x10) -> PKWare (0x08) -> Zlib (0x02) -> Huffman (0x01)
	if (compression_flags & 0x10) {
		result = bzip2_decompress(result);
		compression_flags &= ~0x10;

		if (compression_flags == 0)
			return result;
	}

	if (compression_flags & 0x08) {
		if (expected_size == 0)
			throw std::runtime_error("PKWare decompression requires expectedSize parameter");

		result = pkware_dcl_explode(result, expected_size);
		compression_flags &= ~0x08;

		if (compression_flags == 0)
			return result;
	}

	if (compression_flags & 0x02) {
		result = inflateData(result);
		compression_flags &= ~0x02;

		if (compression_flags == 0)
			return result;
	}

	if (compression_flags & 0x01) {
		result = huffman_decomp(result);
		compression_flags &= ~0x01;

		if (compression_flags == 0)
			return result;
	}

	if (compression_flags & 0x80)
		throw std::runtime_error("ADPCM Stereo compression (0x80) not yet supported");

	if (compression_flags & 0x40)
		throw std::runtime_error("ADPCM Mono compression (0x40) not yet supported");

	if (compression_flags != 0)
		throw std::runtime_error(std::format("unhandled compression flags remaining: 0x{:x}", compression_flags));

	return result;
}

std::vector<uint8_t> MPQArchive::inflateData(std::span<const uint8_t> data) {
	try {
		z_stream strm{};
		strm.next_in = const_cast<Bytef*>(data.data());
		strm.avail_in = static_cast<uInt>(data.size());

		if (inflateInit(&strm) != Z_OK)
			throw std::runtime_error("Failed to initialize zlib inflate");

		std::vector<uint8_t> result;
		std::array<uint8_t, 32768> buffer;

		int ret;
		do {
			strm.next_out = buffer.data();
			strm.avail_out = static_cast<uInt>(buffer.size());

			ret = inflate(&strm, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				inflateEnd(&strm);
				throw std::runtime_error("zlib inflate error");
			}

			const size_t have = buffer.size() - strm.avail_out;
			result.insert(result.end(), buffer.begin(), buffer.begin() + have);
		} while (ret != Z_STREAM_END);

		inflateEnd(&strm);
		return result;
	} catch (const std::exception& e) {
		std::cerr << "decompression error: " << e.what() << '\n';
		throw;
	}
}

std::optional<std::vector<uint8_t>> MPQArchive::extractFile(const std::string& filename) {
	const HashTableEntry* hash_entry = getHashTableEntry(filename);
	if (!hash_entry)
		return std::nullopt;

	const auto& block_entry = blockTable[hash_entry->blockTableIndex];

	if (!(block_entry.flags & MPQ_FILE_EXISTS))
		return std::nullopt;

	if (block_entry.archivedSize == 0)
		return std::vector<uint8_t>{};

	const uint64_t file_offset = static_cast<uint64_t>(block_entry.offset) + header.offset;
	auto file_data = readBytes(file_offset, block_entry.archivedSize);

	uint32_t encryption_seed = 0;
	bool is_encrypted = false;

	if (block_entry.flags & MPQ_FILE_ENCRYPTED) {
		is_encrypted = true;
		const auto path_separator_index = filename.rfind('\\');
		const std::string file_name_only = (path_separator_index != std::string::npos)
			? filename.substr(path_separator_index + 1) : filename;
		encryption_seed = hash(file_name_only, HashType::TABLE);

		if (block_entry.flags & MPQ_FILE_FIX_KEY)
			encryption_seed = (encryption_seed + block_entry.offset) ^ block_entry.size;
	}

	// check if file is actually stored as single unit even without flag
	// this happens when archivedSize == size and no compression
	const bool is_single_unit = (block_entry.flags & MPQ_FILE_SINGLE_UNIT) ||
	                            (block_entry.archivedSize == block_entry.size && !(block_entry.flags & MPQ_FILE_COMPRESS));

	if (is_single_unit) {
		if (is_encrypted)
			file_data = decrypt(file_data, encryption_seed);

		if (block_entry.flags & MPQ_FILE_COMPRESS) {
			if (block_entry.size > block_entry.archivedSize)
				file_data = decompress(file_data, block_entry.size);
		}
	} else {
		const uint32_t sector_size = 512u << header.sectorSizeShift;
		uint32_t sectors = static_cast<uint32_t>(std::ceil(static_cast<double>(block_entry.size) / sector_size));

		const bool has_crc = !!(block_entry.flags & MPQ_FILE_SECTOR_CRC);
		if (has_crc)
			sectors++;

		std::vector<uint32_t> positions;
		for (uint32_t i = 0; i <= sectors; i++)
			positions.push_back(readU32LE(file_data.data() + i * 4));

		const uint32_t expected_first_pos = (sectors + 1) * 4;
		const bool is_position_table_encrypted = positions[0] != expected_first_pos;

		if (is_position_table_encrypted) {
			std::vector<uint8_t> position_table_data(file_data.begin(), file_data.begin() + (sectors + 1) * 4);

			if (encryption_seed == 0) {
				encryption_seed = detectFileSeed(position_table_data, expected_first_pos);
				if (encryption_seed == 0) {
					// plaintext attack failed, compute from filename
					const auto path_separator_index = filename.rfind('\\');
					const std::string file_name_only = (path_separator_index != std::string::npos)
						? filename.substr(path_separator_index + 1) : filename;
					encryption_seed = hash(file_name_only, HashType::TABLE);

					if (block_entry.flags & MPQ_FILE_FIX_KEY)
						encryption_seed = (encryption_seed + block_entry.offset) ^ block_entry.size;
				}
				is_encrypted = true;
			} else {
				const uint32_t detected_seed = detectFileSeed(position_table_data, expected_first_pos);
				if (detected_seed != 0)
					encryption_seed = detected_seed;
			}

			std::vector<uint8_t> encrypted_positions((sectors + 1) * 4);
			for (uint32_t i = 0; i <= sectors; i++)
				writeU32LE(encrypted_positions.data() + i * 4, positions[i]);

			auto decrypted_positions = decrypt(encrypted_positions, encryption_seed);

			positions.clear();
			for (uint32_t i = 0; i <= sectors; i++)
				positions.push_back(readU32LE(decrypted_positions.data() + i * 4));

			encryption_seed = encryption_seed + 1;
		}

		std::vector<std::vector<uint8_t>> result;
		uint32_t sector_bytes_left = block_entry.size;
		const size_t num_sectors = positions.size() - (has_crc ? 2 : 1);

		for (size_t i = 0; i < num_sectors; i++) {
			std::vector<uint8_t> sector(file_data.begin() + positions[i], file_data.begin() + positions[i + 1]);

			if (is_encrypted && block_entry.size > 3) {
				const uint32_t sector_seed = encryption_seed + static_cast<uint32_t>(i);
				sector = decrypt(sector, sector_seed);
			}

			if (block_entry.flags & MPQ_FILE_COMPRESS) {
				const uint32_t expected_sector_size = std::min(sector_size, sector_bytes_left);
				if (sector.size() != expected_sector_size) {
					sector = decompress(sector, expected_sector_size);
				}
			}

			sector_bytes_left -= static_cast<uint32_t>(sector.size());
			result.push_back(std::move(sector));
		}

		size_t total_length = 0;
		for (const auto& arr : result)
			total_length += arr.size();

		std::vector<uint8_t> combined(total_length);
		size_t off = 0;

		for (const auto& arr : result) {
			std::memcpy(combined.data() + off, arr.data(), arr.size());
			off += arr.size();
		}
		file_data = std::move(combined);
	}

	return file_data;
}

ArchiveInfo MPQArchive::getInfo() const {
	return {
		header.formatVersion,
		header.archivedSize,
		files.size(),
		header.hashTableEntries,
		header.blockTableEntries,
	};
}

std::vector<HashTableEntry> MPQArchive::getValidHashEntries() const {
	std::vector<HashTableEntry> result;
	for (const auto& entry : hashTable) {
		if (entry.blockTableIndex != HASH_ENTRY_EMPTY &&
		    entry.blockTableIndex != HASH_ENTRY_DELETED &&
		    entry.blockTableIndex < blockTable.size() &&
		    entry.hashA != HASH_ENTRY_EMPTY &&
		    entry.hashB != HASH_ENTRY_EMPTY) {
			result.push_back(entry);
		}
	}
	return result;
}

std::vector<std::pair<uint32_t, BlockTableEntry>> MPQArchive::getValidBlockEntries() const {
	std::vector<std::pair<uint32_t, BlockTableEntry>> result;
	for (uint32_t i = 0; i < blockTable.size(); i++) {
		if (blockTable[i].flags & MPQ_FILE_EXISTS)
			result.push_back({ i, blockTable[i] });
	}
	return result;
}

std::optional<std::vector<uint8_t>> MPQArchive::extractFileByBlockIndex(uint32_t blockIndex) {
	if (blockIndex >= blockTable.size())
		return std::nullopt;

	const auto& block_entry = blockTable[blockIndex];

	if (!(block_entry.flags & MPQ_FILE_EXISTS))
		return std::nullopt;

	if (block_entry.archivedSize == 0)
		return std::vector<uint8_t>{};

	const uint64_t file_offset = static_cast<uint64_t>(block_entry.offset) + header.offset;
	auto file_data = readBytes(file_offset, block_entry.archivedSize);

	if (block_entry.flags & MPQ_FILE_ENCRYPTED)
		return std::nullopt; // todo: MPQ_FILE_FIX_KEY?

	if (block_entry.flags & MPQ_FILE_SINGLE_UNIT) {
		if (block_entry.flags & MPQ_FILE_COMPRESS) {
			if (block_entry.size > block_entry.archivedSize)
				file_data = decompress(file_data, block_entry.size);
		}
	} else {
		const uint32_t sector_size = 512u << header.sectorSizeShift;
		uint32_t sectors = static_cast<uint32_t>(std::ceil(static_cast<double>(block_entry.size) / sector_size));

		const bool has_crc = !!(block_entry.flags & MPQ_FILE_SECTOR_CRC);
		if (has_crc)
			sectors++;

		std::vector<uint32_t> positions;
		for (uint32_t i = 0; i <= sectors; i++)
			positions.push_back(readU32LE(file_data.data() + i * 4));

		std::vector<std::vector<uint8_t>> result;
		uint32_t sector_bytes_left = block_entry.size;
		const size_t num_sectors = positions.size() - (has_crc ? 2 : 1);

		for (size_t i = 0; i < num_sectors; i++) {
			std::vector<uint8_t> sector(file_data.begin() + positions[i], file_data.begin() + positions[i + 1]);

			if (block_entry.flags & MPQ_FILE_COMPRESS) {
				const uint32_t expected_sector_size = std::min(sector_size, sector_bytes_left);
				if (sector.size() != expected_sector_size) {
					sector = decompress(sector, expected_sector_size);
				}
			}

			sector_bytes_left -= static_cast<uint32_t>(sector.size());
			result.push_back(std::move(sector));
		}

		size_t total_length = 0;
		for (const auto& arr : result)
			total_length += arr.size();

		std::vector<uint8_t> combined(total_length);
		size_t off = 0;

		for (const auto& arr : result) {
			std::memcpy(combined.data() + off, arr.data(), arr.size());
			off += arr.size();
		}
		file_data = std::move(combined);
	}

	return file_data;
}

} // namespace mpq

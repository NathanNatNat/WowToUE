/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#include "casc-source.h"
#include "blte-reader.h"
#include "listfile.h"
#include "dbd-manifest.h"
#include "../log.h"
#include "../core.h"
#include "../constants.h"
#include "locale-flags.h"
#include "content-flags.h"
#include "install-manifest.h"
#include "../buffer.h"
#include "../mmap.h"

#include <stdexcept>
#include <string>
#include <format>
#include <unordered_set>
#include <future>

namespace casc {

CASC::CASC(bool isRemote)
	: isRemote(isRemote)
{
	// Listen for configuration changes to cascLocale.
	const auto& configJson = core::view->config;
	if (configJson.contains("cascLocale")) {
		auto& localeVal = configJson["cascLocale"];
		if (localeVal.is_number()) {
			locale = localeVal.get<uint32_t>();
		} else {
			logging::write("Invalid locale set in configuration, defaulting to enUS");
			locale = locale_flags::enUS;
		}
	} else {
		locale = locale_flags::enUS;
	}

	unhookConfigId = core::events.on("config-change", [this]() {
		const auto& cfg = core::view->config;
		if (cfg.contains("cascLocale")) {
			auto& localeVal = cfg["cascLocale"];
			if (localeVal.is_number()) {
				locale = localeVal.get<uint32_t>();
			} else {
				logging::write("Invalid locale set in configuration, defaulting to enUS");
				locale = locale_flags::enUS;
			}
		}
	});
}

CASC::~CASC() = default;

/**
 * Provides an array of fileDataIDs that match the current locale.
 * @returns Array of file data IDs.
 */
std::vector<uint32_t> CASC::getValidRootEntries() {
	std::vector<uint32_t> entries;

	for (const auto& [fileDataID, entry] : rootEntries) {
		bool include = false;

		for (const auto& [rootTypeIdx, _key] : entry) {
			const auto& rootType = rootTypes[rootTypeIdx];
			if ((rootType.localeFlags & locale) && ((rootType.contentFlags & content_flags::LowViolence) == 0)) {
				include = true;
				break;
			}
		}

		if (include)
			entries.push_back(fileDataID);
	}

	return entries;
}

/**
 * Retrieves the install manifest for this CASC instance.
 * @returns InstallManifest
 */
InstallManifest CASC::getInstallManifest() {
	const auto& installStr = buildConfig.at("install");

	std::vector<std::string> installKeys;
	{
		std::istringstream iss(installStr);
		std::string token;
		while (iss >> token)
			installKeys.push_back(token);
	}

	std::string installKey;
	if (installKeys.size() == 1) {
		auto it = encodingKeys.find(installKeys[0]);
		if (it != encodingKeys.end())
			installKey = it->second;
	} else {
		installKey = installKeys[1];
	}

	BufferWrapper raw = isRemote ? getDataFile(formatCDNKey(installKey)) : getDataFileWithRemoteFallback(installKey);
	BLTEReader manifest(std::move(raw), installKey);

	return InstallManifest(manifest);
}

/**
 * Check if a file exists by its fileDataID.
 * @param fileDataID
 * @returns true if file exists for current locale.
 */
bool CASC::fileExists(uint32_t fileDataID) {
	auto rootIt = rootEntries.find(fileDataID);
	if (rootIt == rootEntries.end())
		return false;

	for (const auto& [rootTypeIdx, _key] : rootIt->second) {
		const auto& rootType = rootTypes[rootTypeIdx];
		if ((rootType.localeFlags & locale) && ((rootType.contentFlags & content_flags::LowViolence) == 0))
			return true;
	}

	return false;
}

/**
 * Obtain a file by it's fileDataID.
 * @param fileDataID
 */
std::string CASC::getFile(uint32_t fileDataID) {
	auto rootIt = rootEntries.find(fileDataID);
	if (rootIt == rootEntries.end())
		throw std::runtime_error("fileDataID does not exist in root: " + std::to_string(fileDataID));

	std::string contentKey;
	bool found = false;
	for (const auto& [rootTypeIdx, key] : rootIt->second) {
		const auto& rootType = rootTypes[rootTypeIdx];

		// Select the first root entry that has a matching locale and no LowViolence flag set.
		if ((rootType.localeFlags & locale) && ((rootType.contentFlags & content_flags::LowViolence) == 0)) {
			contentKey = key;
			found = true;
			break;
		}
	}

	if (!found)
		throw std::runtime_error("No root entry found for locale: " + std::to_string(locale));

	auto encIt = encodingKeys.find(contentKey);
	if (encIt == encodingKeys.end())
		throw std::runtime_error("No encoding entry found: " + contentKey);

	// This underlying implementation returns the encoding key rather than a
	// data file, allowing CASCLocal and CASCRemote to implement readers.
	return encIt->second;
}

/**
 * @param contentKey
 * @returns encoding key string
 */
std::string CASC::getEncodingKeyForContentKey(const std::string& contentKey) {
	auto encIt = encodingKeys.find(contentKey);
	if (encIt == encodingKeys.end())
		throw std::runtime_error("No encoding entry found: " + contentKey);

	// This underlying implementation returns the encoding key rather than a
	// data file, allowing CASCLocal and CASCRemote to implement readers.
	return encIt->second;
}

/**
 * Get encoding info for a file by fileDataID for CDN streaming.
 * Returns FileEncodingInfo or std::nullopt.
 * @param fileDataID
 */
std::optional<FileEncodingInfo> CASC::getFileEncodingInfo(uint32_t fileDataID) {
	try {
		const std::string encodingKey = getFile(fileDataID);
		return FileEncodingInfo{ encodingKey, std::nullopt };
	} catch (...) {
		return std::nullopt;
	}
}

/**
 * Obtain a file by a filename.
 * fileName must exist in the loaded listfile.
 * @param fileName
 * @param partialDecrypt
 * @param suppressLog
 * @param supportFallback
 * @param forceFallback
 */
BLTEReader CASC::getFileByName(const std::string& fileName, bool partialDecrypt,
	bool suppressLog, bool supportFallback, bool forceFallback)
{
	std::optional<uint32_t> fileDataID;

	// If filename is "unknown/<fdid>", skip listfile lookup
	if (fileName.starts_with("unknown/") && fileName.find('.') == std::string::npos) {
		auto slashPos = fileName.find('/');
		if (slashPos != std::string::npos)
			fileDataID = static_cast<uint32_t>(std::stoi(fileName.substr(slashPos + 1)));
	} else {
		// try dbd manifest first for db2 files
		if (fileName.starts_with("DBFilesClient/") && fileName.ends_with(".db2")) {
			std::string table_name = fileName.substr(14, fileName.length() - 14 - 4);
			auto dbdId = dbd_manifest::getByTableName(table_name);
			if (dbdId.has_value())
				fileDataID = dbdId.value();
		}

		// fallback to listfile
		if (!fileDataID.has_value())
			fileDataID = listfile::getByFilename(fileName);
	}

	if (!fileDataID.has_value())
		throw std::runtime_error("File not mapping in listfile: " + fileName);

	return getFileAsBLTE(fileDataID.value(), partialDecrypt, suppressLog, supportFallback, forceFallback);
}

/**
 * get memory-mapped file by fileDataID.
 * ensures file is in cache (unwrapped from BLTE), then returns bufferwrapper wrapping mmap.
 * @param fileDataID
 * @param suppressLog
 * @returns BufferWrapper wrapper around memory-mapped file
 */
BufferWrapper CASC::getVirtualFileByID(uint32_t fileDataID, bool suppressLog) {
	auto rootIt = rootEntries.find(fileDataID);
	if (rootIt == rootEntries.end())
		throw std::runtime_error("fileDataID does not exist in root: " + std::to_string(fileDataID));

	std::string contentKey;
	bool found = false;
	for (const auto& [rootTypeIdx, key] : rootIt->second) {
		const auto& rootType = rootTypes[rootTypeIdx];

		if ((rootType.localeFlags & locale) && ((rootType.contentFlags & content_flags::LowViolence) == 0)) {
			contentKey = key;
			found = true;
			break;
		}
	}

	if (!found)
		throw std::runtime_error("no root entry found for locale: " + std::to_string(locale));

	auto encIt = encodingKeys.find(contentKey);
	if (encIt == encodingKeys.end())
		throw std::runtime_error("no encoding entry found: " + contentKey);

	const std::string& encodingKey = encIt->second;
	const std::string cachedPath = _ensureFileInCache(encodingKey, fileDataID, suppressLog);

	auto* mmap_obj = mmap_util::create_virtual_file();
	if (!mmap_obj->map(cachedPath))
		throw std::runtime_error("failed to map file: " + mmap_obj->lastError);

	return BufferWrapper::fromMmap(mmap_obj->data, mmap_obj->size);
}

/**
 * get memory-mapped file by filename.
 * @param fileName
 * @param suppressLog
 * @returns BufferWrapper wrapper around memory-mapped file
 */
BufferWrapper CASC::getVirtualFileByName(const std::string& fileName, bool suppressLog) {
	std::optional<uint32_t> fileDataID;

	if (fileName.starts_with("unknown/") && fileName.find('.') == std::string::npos) {
		auto slashPos = fileName.find('/');
		if (slashPos != std::string::npos)
			fileDataID = static_cast<uint32_t>(std::stoi(fileName.substr(slashPos + 1)));
	} else {
		// try dbd manifest first for db2 files
		if (fileName.starts_with("DBFilesClient/") && fileName.ends_with(".db2")) {
			std::string table_name = fileName.substr(14, fileName.length() - 14 - 4);
			auto dbdId = dbd_manifest::getByTableName(table_name);
			if (dbdId.has_value())
				fileDataID = dbdId.value();
		}

		// fallback to listfile
		if (!fileDataID.has_value())
			fileDataID = listfile::getByFilename(fileName);
	}

	if (!fileDataID.has_value())
		throw std::runtime_error("file not mapping in listfile: " + fileName);

	return getVirtualFileByID(fileDataID.value(), suppressLog);
}

/**
 * Prepare listfile data before loading.
 * Ensures preloading is complete to avoid race conditions.
 */
void CASC::prepareListfile() {
	core::progressLoadingScreen("Preparing listfiles...");
	listfile::prepareListfile();
}

/**
 * prepare dbd manifest before loading.
 * ensures preloading is complete.
 */
void CASC::prepareDBDManifest() {
	core::progressLoadingScreen("Loading DBD manifest...");
	dbd_manifest::prepareManifest();
}

/**
 * Load the listfile for selected build.
 * @param buildKey
 */
void CASC::loadListfile(const std::string& buildKey) {
	core::progressLoadingScreen("Loading listfiles");

	std::unordered_set<uint32_t> rootEntryIds;
	rootEntryIds.reserve(rootEntries.size());
	for (const auto& [fileDataID, _entry] : rootEntries)
		rootEntryIds.insert(fileDataID);

	listfile::applyPreload(rootEntryIds);
}

/**
 * Returns an array of model formats to display.
 * @returns Array of extension filters.
 */
std::vector<listfile::ExtFilter> CASC::getModelFormats() {
	// Filters for the model viewer depending on user settings.
	std::vector<listfile::ExtFilter> modelExt;

	const auto& cfg = core::view->config;

	if (cfg.contains("modelsShowM3") && cfg["modelsShowM3"].get<bool>())
		modelExt.emplace_back(".m3");

	if (cfg.contains("modelsShowM2") && cfg["modelsShowM2"].get<bool>())
		modelExt.emplace_back(".m2");

	if (cfg.contains("modelsShowWMO") && cfg["modelsShowWMO"].get<bool>())
		modelExt.emplace_back(".wmo", constants::LISTFILE_MODEL_FILTER());

	return modelExt;
}

/**
 * Parse entries from a root file.
 * @param data
 * @param hash
 * @returns Number of root entries.
 */
size_t CASC::parseRootFile(BufferWrapper data, const std::string& hash) {
	BLTEReader root(std::move(data), hash);

	const uint32_t magic = root.readUInt32LE();

	rootEntries.reserve(2'000'000);

	if (magic == ROOT_MAGIC) { // 8.2
		uint32_t headerSize = root.readUInt32LE();
		uint32_t version = root.readUInt32LE();

		if (headerSize != 0x18) {
			version = 0; // This will break with future header size increases.
		} else {
			if (version != 1 && version != 2)
				throw std::runtime_error("Unknown root version: " + std::to_string(version));
		}

		uint32_t totalFileCount;
		uint32_t namedFileCount;

		if (version == 0)
		{
			totalFileCount = headerSize;
			namedFileCount = version;
			headerSize = 12;
		}
		else
		{
			totalFileCount = root.readUInt32LE();
			namedFileCount = root.readUInt32LE();
		}

		root.seek(headerSize);

		const bool allowNamelessFiles = totalFileCount != namedFileCount;

		while (root.remainingBytes() > 0) {
			const uint32_t numRecords = root.readUInt32LE();

			uint32_t contentFlags = 0;
			uint32_t localeFlags = 0;

			if (version == 0 || version == 1) {
				contentFlags = root.readUInt32LE();
				localeFlags = root.readUInt32LE();
			} else if (version == 2) {
				localeFlags = root.readUInt32LE();
				const uint32_t cflags1 = root.readUInt32LE();
				const uint32_t cflags2 = root.readUInt32LE();
				const uint8_t cflags3 = root.readUInt8();
				contentFlags = cflags1 | cflags2 | (static_cast<uint32_t>(cflags3) << 17);
			}

			std::vector<int32_t> fileDataIDs(numRecords);

			int32_t fileDataID = 0;
			for (uint32_t i = 0; i < numRecords; i++) {
				const int32_t nextID = fileDataID + root.readInt32LE();
				fileDataIDs[i] = nextID;
				fileDataID = nextID + 1;
			}

			// Parse MD5 content keys for entries.
			for (uint32_t i = 0; i < numRecords; i++) {
				const uint32_t fdid = static_cast<uint32_t>(fileDataIDs[i]);
				auto& entry = rootEntries[fdid];
				entry[rootTypes.size()] = root.readHexString(16);
			}

			// Skip lookup hashes for entries.
			if (!(allowNamelessFiles && (contentFlags & content_flags::NoNameHash)))
				root.move(8 * numRecords);

			// Push the rootType after parsing the block so that
			// rootTypes.size() can be used for the type index above.
			rootTypes.push_back({ contentFlags, localeFlags });
		}
	} else { // Classic
		root.seek(0);
		while (root.remainingBytes() > 0) {
			const uint32_t numRecords = root.readUInt32LE();

			const uint32_t contentFlags = root.readUInt32LE();
			const uint32_t localeFlags = root.readUInt32LE();

			std::vector<int32_t> fileDataIDs(numRecords);

			int32_t fileDataID = 0;
			for (uint32_t i = 0; i < numRecords; i++) {
				const int32_t nextID = fileDataID + root.readInt32LE();
				fileDataIDs[i] = nextID;
				fileDataID = nextID + 1;
			}

			// Parse MD5 content keys for entries.
			for (uint32_t i = 0; i < numRecords; i++) {
				const std::string key = root.readHexString(16);
				root.move(8); // hash

				const uint32_t fdid = static_cast<uint32_t>(fileDataIDs[i]);
				auto& entry = rootEntries[fdid];
				entry[rootTypes.size()] = key;
			}

			// Push the rootType after parsing the block so that
			// rootTypes.size() can be used for the type index above.
			rootTypes.push_back({ contentFlags, localeFlags });
		}
	}

	return rootEntries.size();
}

/**
 * Parse entries from an encoding file.
 * @param data
 * @param hash
 */
void CASC::parseEncodingFile(BufferWrapper data, const std::string& hash) {
	BLTEReader encoding(std::move(data), hash);

	const uint16_t magic = encoding.readUInt16LE();
	if (magic != ENC_MAGIC)
		throw std::runtime_error("Invalid encoding magic: " + std::to_string(magic));

	encoding.move(1); // version
	const uint8_t hashSizeCKey = encoding.readUInt8();
	const uint8_t hashSizeEKey = encoding.readUInt8();
	const int32_t cKeyPageSize = encoding.readInt16BE() * 1024;
	encoding.move(2); // eKeyPageSize
	const int32_t cKeyPageCount = encoding.readInt32BE();
	encoding.move(4 + 1); // eKeyPageCount + unk11
	const int32_t specBlockSize = encoding.readInt32BE();

	encoding.move(specBlockSize + (cKeyPageCount * (hashSizeCKey + 16)));

	encodingKeys.reserve(3'000'000);
	encodingSizes.reserve(3'000'000);

	const size_t pagesStart = encoding.offset();
	for (int32_t i = 0; i < cKeyPageCount; i++) {
		const size_t pageStart = pagesStart + (static_cast<size_t>(cKeyPageSize) * i);
		encoding.seek(pageStart);

		while (encoding.offset() < (pageStart + pagesStart)) {
			const uint8_t keysCount = encoding.readUInt8();
			if (keysCount == 0)
				break;

			const int64_t size = encoding.readInt40BE();
			const std::string cKey = encoding.readHexString(hashSizeCKey);

			encodingSizes[cKey] = size;
			encodingKeys[cKey] = encoding.readHexString(hashSizeEKey);

			encoding.move(hashSizeEKey * (keysCount - 1));
		}
	}
}

std::future<InstallManifest> CASC::getInstallManifestAsync() {
	return std::async(std::launch::async, [this]() {
		return getInstallManifest();
	});
}

std::future<std::string> CASC::getFileAsync(uint32_t fileDataID) {
	return std::async(std::launch::async, [this, fileDataID]() {
		return getFile(fileDataID);
	});
}

std::future<std::optional<FileEncodingInfo>> CASC::getFileEncodingInfoAsync(uint32_t fileDataID) {
	return std::async(std::launch::async, [this, fileDataID]() {
		return getFileEncodingInfo(fileDataID);
	});
}

std::future<BLTEReader> CASC::getFileByNameAsync(const std::string& fileName, bool partialDecrypt,
	bool suppressLog, bool supportFallback, bool forceFallback)
{
	return std::async(std::launch::async, [this, fileName, partialDecrypt, suppressLog, supportFallback, forceFallback]() {
		return getFileByName(fileName, partialDecrypt, suppressLog, supportFallback, forceFallback);
	});
}

std::future<BufferWrapper> CASC::getVirtualFileByIDAsync(uint32_t fileDataID, bool suppressLog) {
	return std::async(std::launch::async, [this, fileDataID, suppressLog]() {
		return getVirtualFileByID(fileDataID, suppressLog);
	});
}

std::future<BufferWrapper> CASC::getVirtualFileByNameAsync(const std::string& fileName, bool suppressLog) {
	return std::async(std::launch::async, [this, fileName, suppressLog]() {
		return getVirtualFileByName(fileName, suppressLog);
	});
}

std::future<void> CASC::prepareListfileAsync() {
	return std::async(std::launch::async, [this]() {
		prepareListfile();
	});
}

std::future<void> CASC::prepareDBDManifestAsync() {
	return std::async(std::launch::async, [this]() {
		prepareDBDManifest();
	});
}

std::future<void> CASC::loadListfileAsync(const std::string& buildKey) {
	return std::async(std::launch::async, [this, buildKey]() {
		loadListfile(buildKey);
	});
}

std::future<size_t> CASC::parseRootFileAsync(BufferWrapper data, const std::string& hash) {
	return std::async(std::launch::async, [this, data = std::move(data), hash]() mutable {
		return parseRootFile(std::move(data), hash);
	});
}

std::future<void> CASC::parseEncodingFileAsync(BufferWrapper data, const std::string& hash) {
	return std::async(std::launch::async, [this, data = std::move(data), hash]() mutable {
		parseEncodingFile(std::move(data), hash);
	});
}

/**
 * Run any necessary clean-up once a CASC instance is no longer
 * needed. At this point, the instance must be made eligible for GC.
 */
void CASC::cleanup() {
	core::events.off("config-change", unhookConfigId);
}

std::string CASC::formatCDNKey(const std::string& key) {
	return key.substr(0, 2) + "/" + key.substr(2, 2) + "/" + key;
}

BufferWrapper CASC::getDataFile(const std::string& /*file*/) {
	throw std::runtime_error("getDataFile not implemented in base CASC class");
}

BufferWrapper CASC::getDataFileWithRemoteFallback(const std::string& /*key*/, bool /*forceFallback*/) {
	throw std::runtime_error("getDataFileWithRemoteFallback not implemented in base CASC class");
}

std::string CASC::_ensureFileInCache(const std::string& /*encodingKey*/, uint32_t /*fileDataID*/, bool /*suppressLog*/) {
	throw std::runtime_error("_ensureFileInCache not implemented in base CASC class");
}

} // namespace casc

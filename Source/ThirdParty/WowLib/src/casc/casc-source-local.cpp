/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "casc-source-local.h"
#include "../log.h"
#include "../constants.h"
#include "casc-source.h"
#include "version-config.h"
#include "cdn-config.h"
#include "../buffer.h"
#include "build-cache.h"
#include "blte-reader.h"
#include "blte-stream-reader.h"
#include "listfile.h"
#include "../core.h"
#include "../generics.h"
#include "casc-source-remote.h"
#include "cdn-resolver.h"
#include "tact-keys.h"

#include <stdexcept>
#include <string>
#include <format>
#include <regex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <future>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace casc {

namespace {
std::string dumpMap(const std::unordered_map<std::string, std::string>& m) {
	nlohmann::json j;
	for (const auto& [k, v] : m)
		j[k] = v;
	return j.dump();
}

std::string dumpMaps(const std::vector<std::unordered_map<std::string, std::string>>& v) {
	nlohmann::json j = nlohmann::json::array();
	for (const auto& m : v) {
		nlohmann::json obj;
		for (const auto& [k, val] : m)
			obj[k] = val;
		j.push_back(obj);
	}
	return j.dump();
}
} // anonymous namespace

CASCLocal::CASCLocal(const std::string& dir)
	: CASC(false), dir(dir)
{
	dataDir = fs::path(dir) / std::string(constants::BUILD::DATA_DIR);
	storageDir = dataDir / "data";
}

/**
 * Initialize local CASC source.
 */
void CASCLocal::init() {
	logging::write("Initializing local CASC installation: " + dir);

	const auto buildInfo = fs::path(dir) / std::string(constants::BUILD::MANIFEST);

	std::ifstream ifs(buildInfo);
	if (!ifs.is_open())
		throw std::runtime_error("Failed to open build info: " + buildInfo.string());

	std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	auto config = parseVersionConfig(content);

	// Filter known products.
	builds.clear();
	for (auto& entry : config) {
		if (!entry.count("Product"))
			continue;

		const std::string& product = entry["Product"];
		bool known = false;
		for (const auto& p : constants::PRODUCTS) {
			if (p.product == product) {
				known = true;
				break;
			}
		}
		if (known)
			builds.push_back(entry);
	}

	logging::write(dumpMaps(builds));
}

/**
 * Obtain a file by it's fileDataID.
 * @param fileDataID
 * @param partialDecryption
 * @param suppressLog
 * @param supportFallback
 * @param forceFallback
 * @param contentKey
 */
BLTEReader CASCLocal::getFileAsBLTE(uint32_t fileDataID, bool partialDecryption,
	bool suppressLog, bool supportFallback, bool forceFallback, const std::string& contentKey)
{
	if (!suppressLog)
		logging::write(std::format("Loading local CASC file {} ({})", fileDataID, listfile::getByID(fileDataID).value_or("")));

	const std::string encodingKey = !contentKey.empty() ? CASC::getEncodingKeyForContentKey(contentKey) : CASC::getFile(fileDataID);
	BufferWrapper data = supportFallback ? getDataFileWithRemoteFallback(encodingKey, forceFallback) : getDataFile(encodingKey);
	return BLTEReader(std::move(data), encodingKey, partialDecryption);
}

/**
 * Get a streaming reader for a file by its fileDataID.
 * @param fileDataID
 * @param partialDecrypt
 * @param suppressLog
 * @param supportFallback
 * @param forceFallback
 * @param contentKey
 * @returns BLTEStreamReader
 */
BLTEStreamReader CASCLocal::getFileStream(uint32_t fileDataID, bool partialDecrypt,
	bool suppressLog, bool supportFallback, bool forceFallback, const std::string& contentKey)
{
	if (!suppressLog)
		logging::write(std::format("Creating stream for local CASC file {} ({})", fileDataID, listfile::getByID(fileDataID).value_or("")));

	// if fallback forced or not supported, use remote streaming
	if (forceFallback || !supportFallback) {
		if (!remote)
			initializeRemoteCASC();

		return remote->getFileStream(fileDataID, partialDecrypt, suppressLog, contentKey);
	}

	const std::string encodingKey = !contentKey.empty() ? CASC::getEncodingKeyForContentKey(contentKey) : CASC::getFile(fileDataID);
	auto entryIt = localIndexes.find(encodingKey.substr(0, 18));

	if (entryIt == localIndexes.end()) {
		// file doesn't exist locally, fallback to remote
		if (!supportFallback)
			throw std::runtime_error("file does not exist in local data: " + encodingKey);

		if (!remote)
			initializeRemoteCASC();

		return remote->getFileStream(fileDataID, partialDecrypt, suppressLog, contentKey);
	}

	const auto& entry = entryIt->second;

	// read blte header from local file
	BufferWrapper headerData = generics::readFile(
		formatDataPath(entry.index),
		entry.offset + 0x1e,
		std::min(4096, entry.size - 0x1e)
	);

	// check if valid blte
	if (!BLTEReader::checkBLTE(headerData)) {
		if (!supportFallback)
			throw std::runtime_error("local data file is not a valid BLTE");

		if (!remote)
			initializeRemoteCASC();

		return remote->getFileStream(fileDataID, partialDecrypt, suppressLog, contentKey);
	}

	auto metadata = BLTEReader::parseBLTEHeader(headerData, encodingKey, false);

	// create block fetcher function for local files
	const int32_t entryIndex = entry.index;
	const int32_t entryOffset = entry.offset;

	auto blockFetcher = [this, metadata, entryIndex, entryOffset](size_t blockIndex) -> BufferWrapper {
		const auto& block = metadata.blocks[blockIndex];
		const int64_t blockOffset = static_cast<int64_t>(metadata.dataStart) + block.fileOffset;

		return generics::readFile(
			this->formatDataPath(entryIndex),
			entryOffset + 0x1e + blockOffset,
			block.CompSize
		);
	};

	return BLTEStreamReader(encodingKey, metadata, blockFetcher, partialDecrypt);
}

/**
 * Returns a list of available products in the installation.
 * Format example: "PTR: World of Warcraft 8.3.0.32272"
 */
std::vector<ProductEntry> CASCLocal::getProductList() {
	std::vector<ProductEntry> products;
	for (size_t i = 0; i < builds.size(); i++) {
		const auto& entry = builds[i];

		auto productIt = entry.find("Product");
		auto versionIt = entry.find("Version");
		if (productIt == entry.end() || versionIt == entry.end())
			continue;

		std::string_view title;
		for (const auto& p : constants::PRODUCTS) {
			if (p.product == productIt->second) {
				title = p.title;
				break;
			}
		}

		std::string branchStr = entry.at("Branch");
		for (auto& c : branchStr)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

		const std::string label = std::format("{} ({}) {}", title, branchStr, versionIt->second);

		std::regex versionRegex("^(\\d+)\\.");
		std::smatch match;
		const std::string& version = versionIt->second;
		int expansionId = 0;
		if (std::regex_search(version, match, versionRegex))
			expansionId = std::min(std::stoi(match[1].str()) - 1, 12);

		products.push_back({ label, expansionId, static_cast<int>(i) });
	}

	return products;
}

/**
 * Load the CASC interface with the given build.
 * @param buildIndex
 */
void CASCLocal::load(int buildIndex) {
	tact_keys::waitForLoad();

	build = builds[buildIndex];
	logging::write("Loading local CASC build: " + dumpMap(build));

	ownedCache = std::make_unique<BuildCache>(build["BuildKey"]);
	ownedCache->init();
	cache = ownedCache.get();

	core::showLoadingScreen(8);

	loadConfigs();
	loadIndexes();
	loadEncoding();
	loadRoot();

	core::view->casc = this;

	prepareListfile();
	prepareDBDManifest();
	loadListfile(build["BuildKey"]);

	core::hideLoadingScreen();
}

/**
 * Load the BuildConfig from the installation directory.
 */
void CASCLocal::loadConfigs() {
	// Load and parse configs from disk with CDN fallback.
	core::progressLoadingScreen("Fetching build configurations");

	if (generics::fileExists("fakebuildconfig")) {
		std::ifstream ifs("fakebuildconfig");
		std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		buildConfig = parseCDNConfig(content);
		logging::write("WARNING: Using fake build config. No support given for weird stuff happening.");

		// Reconstruct version from the fake config's build name.
		// This is used for e.g. DBD version selection so needs to be correct.
		if (buildConfig.count("buildName")) {
			const std::string& buildName = buildConfig["buildName"];
			auto patchPos = buildName.find("patch");
			if (patchPos != std::string::npos) {
				std::string buildNumber = buildName.substr(0, patchPos);
				auto wowPos = buildNumber.find("WOW-");
				if (wowPos != std::string::npos)
					buildNumber = buildNumber.substr(wowPos + 4);

				std::string patchPart = buildName.substr(patchPos + 5);
				auto underscorePos = patchPart.find('_');
				if (underscorePos != std::string::npos)
					patchPart = patchPart.substr(0, underscorePos);

				build["Version"] = patchPart + "." + buildNumber;
			}
		}
	} else {
		buildConfig = getConfigFileWithRemoteFallback(build["BuildKey"]);
	}

	cdnConfig = getConfigFileWithRemoteFallback(build["CDNKey"]);

	logging::write("BuildConfig: " + dumpMap(buildConfig));
	logging::write("CDNConfig: " + dumpMap(cdnConfig));
}

/**
 * Get config from disk with CDN fallback
 */
std::unordered_map<std::string, std::string> CASCLocal::getConfigFileWithRemoteFallback(const std::string& key) {
	const std::string configPath = formatConfigPath(key);
	if (!generics::fileExists(configPath)) {
		logging::write("Local config file " + key + " does not exist, falling back to CDN...");
		if (!remote)
			initializeRemoteCASC();

		const std::string regionTag = core::view->selectedCDNRegion.at("tag").get<std::string>();
		auto cdnHosts = cdn_resolver::getRankedHosts(regionTag, remote->serverConfig);
		return remote->getCDNConfig(key, cdnHosts);
	} else {
		std::ifstream ifs(configPath);
		std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		return parseCDNConfig(content);
	}
}

/**
 * Load and parse storage indexes from the local installation.
 */
void CASCLocal::loadIndexes() {
	logging::timeLog();
	core::progressLoadingScreen("Loading indexes");

	localIndexes.reserve(2'500'000);
	int indexCount = 0;

	for (const auto& entry : fs::directory_iterator(storageDir)) {
		if (entry.is_regular_file() && entry.path().extension() == ".idx") {
			parseIndex(entry.path());
			indexCount++;
		}
	}

	logging::timeEnd(std::format("Loaded {} entries from {} journal indexes", localIndexes.size(), indexCount));
}

/**
 * Parse a local installation journal index for entries.
 * @param file Path to the index.
 */
void CASCLocal::parseIndex(const fs::path& file) {
	BufferWrapper index = BufferWrapper::readFile(file);

	const int32_t headerHashSize = index.readInt32LE();
	index.move(4); // headerHash uint32
	index.move(headerHashSize); // headerHash byte[headerHashSize]

	index.seek((8 + headerHashSize + 0x0F) & 0xFFFFFFF0); // Next 0x10 boundary.

	const int32_t dataLength = index.readInt32LE();
	index.move(4);

	const int32_t nBlocks = dataLength / 18;
	for (int32_t i = 0; i < nBlocks; i++) {
		const std::string key = index.readHexString(9);
		if (localIndexes.count(key)) {
			index.move(1 + 4 + 4); // idxHigh + idxLow + size
			continue;
		}

		const uint8_t idxHigh = index.readUInt8();
		const int32_t idxLow = index.readInt32BE();

		localIndexes[key] = {
			static_cast<int32_t>((idxHigh << 2) | ((static_cast<uint32_t>(idxLow) & 0xC0000000) >> 30)),
			idxLow & 0x3FFFFFFF,
			index.readInt32LE()
		};
	}
}

/**
 * Load and parse encoding from the local installation.
 */
void CASCLocal::loadEncoding() {
	// Parse encoding file.
	logging::timeLog();

	const std::string& encStr = buildConfig["encoding"];
	std::string encKey;
	{
		auto spacePos = encStr.find(' ');
		if (spacePos != std::string::npos)
			encKey = encStr.substr(spacePos + 1);
		else
			encKey = encStr;
	}

	core::progressLoadingScreen("Loading encoding table");
	BufferWrapper encRaw = getDataFileWithRemoteFallback(encKey);
	parseEncodingFile(std::move(encRaw), encKey);
	logging::timeEnd("Parsed encoding table (" + std::to_string(encodingKeys.size()) + " entries)");
}

/**
 * Load and parse root table from local installation.
 */
void CASCLocal::loadRoot() {
	// Get root key from encoding table.
	auto rootKeyIt = encodingKeys.find(buildConfig["root"]);
	if (rootKeyIt == encodingKeys.end())
		throw std::runtime_error("No encoding entry found for root key");

	const std::string& rootKey = rootKeyIt->second;

	// Parse root file.
	logging::timeLog();
	core::progressLoadingScreen("Loading root file");
	BufferWrapper rootData = getDataFileWithRemoteFallback(rootKey);
	const size_t rootEntryCount = parseRootFile(std::move(rootData), rootKey);
	logging::timeEnd(std::format("Parsed root file ({} entries, {} types)", rootEntryCount, rootTypes.size()));
}

/**
 * Initialize a remote CASC instance to download missing
 * files needed during local initialization.
 */
void CASCLocal::initializeRemoteCASC() {
	const std::string regionTag = core::view->selectedCDNRegion.at("tag").get<std::string>();
	auto remote = std::make_unique<CASCRemote>(regionTag);
	remote->init();

	int buildIndex = -1;
	const std::string& product = build["Product"];
	for (size_t i = 0; i < remote->builds.size(); i++) {
		if (remote->builds[i].count("Product") && remote->builds[i]["Product"] == product) {
			buildIndex = static_cast<int>(i);
			break;
		}
	}

	remote->preload(buildIndex, cache);

	this->remote = std::move(remote);
}

/**
 * Obtain a data file from the local archives.
 * If not stored locally, file will be downloaded from a CDN.
 * @param key
 * @param forceFallback
 */
BufferWrapper CASCLocal::getDataFileWithRemoteFallback(const std::string& key, bool forceFallback) {
	try {
		// If forceFallback is true, we have corrupt local data.
		if (forceFallback)
			throw std::runtime_error("Local data is corrupted, forceFallback set.");

		// Attempt 1: Extract from local archives.
		BufferWrapper local = getDataFile(key);

		if (!BLTEReader::checkBLTE(local))
			throw std::runtime_error("Local data file is not a valid BLTE");

		return local;
	} catch (const std::exception& e) {
		// Attempt 2: Load from cache from previous fallback.
		logging::write("Local data file " + key + " does not exist, falling back to cache...");
		auto cached = cache->getFile(key, constants::CACHE::DIR_DATA().string());
		if (cached.has_value())
			return std::move(cached.value());

		// Attempt 3: Download from CDN.
		logging::write("Local data file " + key + " not cached, falling back to CDN...");
		if (!remote)
			initializeRemoteCASC();

		auto archIt = remote->archives.find(key);
		BufferWrapper data;
		if (archIt != remote->archives.end()) {
			// Archive exists for key, attempt partial remote download.
			logging::write("Local data file " + key + " has archive, attempt partial download...");
			data = remote->getDataFilePartial(remote->formatCDNKey(archIt->second.key), archIt->second.offset, archIt->second.size);
		} else {
			// No archive for this file, attempt direct download.
			logging::write("Local data file " + key + " has no archive, attempting direct download...");
			data = remote->getDataFile(remote->formatCDNKey(key));
		}

		cache->storeFile(key, data, constants::CACHE::DIR_DATA().string());
		return data;
	}
}

/**
 * Obtain a data file from the local archives.
 * @param key
 */
BufferWrapper CASCLocal::getDataFile(const std::string& key) {
	auto entryIt = localIndexes.find(key.substr(0, 18));
	if (entryIt == localIndexes.end())
		throw std::runtime_error("Requested file does not exist in local data: " + key);

	const auto& entry = entryIt->second;
	BufferWrapper data = generics::readFile(formatDataPath(entry.index), entry.offset + 0x1E, entry.size - 0x1E);

	bool isZeroed = true;
	const size_t remaining = data.remainingBytes();
	for (size_t i = 0; i < remaining; i++) {
		if (data.readUInt8() != 0x0) {
			isZeroed = false;
			break;
		}
	}

	if (isZeroed)
		throw std::runtime_error("Requested data file is empty or missing: " + key);

	data.seek(0);
	return data;
}

/**
 * Format a local path to a data archive.
 * 67 -> <install>/Data/data/data.067
 * @param id
 */
std::string CASCLocal::formatDataPath(int32_t id) {
	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(3) << id;
	return (dataDir / "data" / ("data." + oss.str())).string();
}

/**
 * Format a local path to an archive index from the key.
 * 0b45bd2721fd6c86dac2176cbdb7fc5b -> <install>/Data/indices/0b45bd2721fd6c86dac2176cbdb7fc5b.index
 * @param key
 */
std::string CASCLocal::formatIndexPath(const std::string& key) {
	return (dataDir / "indices" / (key + ".index")).string();
}

/**
 * Format a local path to a config file from the key.
 * 0af716e8eca5aeff0a3965d37e934ffa -> <install>/Data/config/0a/f7/0af716e8eca5aeff0a3965d37e934ffa
 * @param key
 */
std::string CASCLocal::formatConfigPath(const std::string& key) {
	return (dataDir / "config" / formatCDNKey(key)).string();
}

/**
 * Format a CDN key for use in local file reading.
 * Path separators used by this method are platform specific.
 * 49299eae4e3a195953764bb4adb3c91f -> 49\29\49299eae4e3a195953764bb4adb3c91f
 * @param key
 */
std::string CASCLocal::formatCDNKey(const std::string& key) {
	return (fs::path(key.substr(0, 2)) / key.substr(2, 2) / key).string();
}

/**
 * ensure file is in cache (unwrapped from BLTE) and return path.
 * @param encodingKey
 * @param fileDataID
 * @param suppressLog
 * @returns path to cached file
 */
std::string CASCLocal::_ensureFileInCache(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog) {
	const std::string cacheFileName = encodingKey + ".data";
	const auto cachedPath = cache->getFilePath(cacheFileName, constants::CACHE::DIR_DATA().string());

	// check if already in cache
	auto cached = cache->getFile(cacheFileName, constants::CACHE::DIR_DATA().string());
	if (cached.has_value())
		return cachedPath.string();

	// retrieve and unwrap from BLTE
	if (!suppressLog)
		logging::write(std::format("caching file {} ({}) for mmap", fileDataID, listfile::getByID(fileDataID).value_or("")));

	BufferWrapper data = getDataFileWithRemoteFallback(encodingKey);
	BLTEReader blte(std::move(data), encodingKey, true);
	blte.processAllBlocks();

	// write to cache
	BufferWrapper blteWrapper = std::move(static_cast<BufferWrapper&>(blte));
	cache->storeFile(cacheFileName, blteWrapper, constants::CACHE::DIR_DATA().string());

	return cachedPath.string();
}

/**
 * Get encoding info for a file by fileDataID for CDN streaming.
 * Returns { enc: string, arc?: { key: string, ofs: number, len: number } }
 * Initializes remote CASC if needed to get archive info for the server.
 * @param fileDataID
 */
std::optional<FileEncodingInfo> CASCLocal::getFileEncodingInfo(uint32_t fileDataID) {
	try {
		const std::string encodingKey = CASC::getFile(fileDataID);

		// initialize remote if not already done
		if (!remote)
			initializeRemoteCASC();

		auto archIt = remote->archives.find(encodingKey);
		if (archIt != remote->archives.end())
			return FileEncodingInfo{ encodingKey, ArchiveInfo{ archIt->second.key, archIt->second.offset, archIt->second.size } };

		return FileEncodingInfo{ encodingKey, std::nullopt };
	} catch (...) {
		return std::nullopt;
	}
}

/**
 * Get the current build ID.
 * @returns {string}
 */
std::string CASCLocal::getBuildName() {
	auto it = build.find("Version");
	return (it != build.end()) ? it->second : "";
}

/**
 * Returns the build configuration key.
 * @returns {string}
 */
std::string CASCLocal::getBuildKey() {
	auto it = build.find("BuildKey");
	return (it != build.end()) ? it->second : "";
}

std::future<void> CASCLocal::initAsync() {
	return std::async(std::launch::async, [this]() {
		init();
	});
}

std::future<BLTEReader> CASCLocal::getFileAsBLTEAsync(uint32_t fileDataID, bool partialDecryption,
	bool suppressLog, bool supportFallback, bool forceFallback, const std::string& contentKey)
{
	return std::async(std::launch::async, [this, fileDataID, partialDecryption, suppressLog, supportFallback, forceFallback, contentKey]() {
		return getFileAsBLTE(fileDataID, partialDecryption, suppressLog, supportFallback, forceFallback, contentKey);
	});
}

std::future<BLTEStreamReader> CASCLocal::getFileStreamAsync(uint32_t fileDataID, bool partialDecrypt,
	bool suppressLog, bool supportFallback, bool forceFallback, const std::string& contentKey)
{
	return std::async(std::launch::async, [this, fileDataID, partialDecrypt, suppressLog, supportFallback, forceFallback, contentKey]() {
		return getFileStream(fileDataID, partialDecrypt, suppressLog, supportFallback, forceFallback, contentKey);
	});
}

std::future<void> CASCLocal::loadAsync(int buildIndex) {
	return std::async(std::launch::async, [this, buildIndex]() {
		load(buildIndex);
	});
}

std::future<void> CASCLocal::loadConfigsAsync() {
	return std::async(std::launch::async, [this]() {
		loadConfigs();
	});
}

std::future<std::unordered_map<std::string, std::string>> CASCLocal::getConfigFileWithRemoteFallbackAsync(const std::string& key) {
	return std::async(std::launch::async, [this, key]() {
		return getConfigFileWithRemoteFallback(key);
	});
}

std::future<void> CASCLocal::loadIndexesAsync() {
	return std::async(std::launch::async, [this]() {
		loadIndexes();
	});
}

std::future<void> CASCLocal::parseIndexAsync(const fs::path& file) {
	return std::async(std::launch::async, [this, file]() {
		parseIndex(file);
	});
}

std::future<void> CASCLocal::loadEncodingAsync() {
	return std::async(std::launch::async, [this]() {
		loadEncoding();
	});
}

std::future<void> CASCLocal::loadRootAsync() {
	return std::async(std::launch::async, [this]() {
		loadRoot();
	});
}

std::future<void> CASCLocal::initializeRemoteCASCAsync() {
	return std::async(std::launch::async, [this]() {
		initializeRemoteCASC();
	});
}

std::future<BufferWrapper> CASCLocal::getDataFileWithRemoteFallbackAsync(const std::string& key, bool forceFallback) {
	return std::async(std::launch::async, [this, key, forceFallback]() {
		return getDataFileWithRemoteFallback(key, forceFallback);
	});
}

std::future<BufferWrapper> CASCLocal::getDataFileAsync(const std::string& key) {
	return std::async(std::launch::async, [this, key]() {
		return getDataFile(key);
	});
}

std::future<std::string> CASCLocal::ensureFileInCacheAsync(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog) {
	return std::async(std::launch::async, [this, encodingKey, fileDataID, suppressLog]() {
		return _ensureFileInCache(encodingKey, fileDataID, suppressLog);
	});
}

std::future<std::optional<FileEncodingInfo>> CASCLocal::getFileEncodingInfoAsync(uint32_t fileDataID) {
	return std::async(std::launch::async, [this, fileDataID]() {
		return getFileEncodingInfo(fileDataID);
	});
}

} // namespace casc

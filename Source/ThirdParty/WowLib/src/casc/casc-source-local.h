/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>
#include <memory>
#include <filesystem>
#include <future>

#include "casc-source.h"
#include "casc-source-remote.h"
#include "build-cache.h"
#include "../buffer.h"

namespace casc {

class BLTEStreamReader;

/**
 * Local index entry for CASC data files.
 */
struct LocalIndexEntry {
	int32_t index;
	int32_t offset;
	int32_t size;
};

/**
 * CASCLocal — CASC source using a local WoW installation.
 *
 * JS equivalent: class CASCLocal extends CASC — module.exports = CASCLocal
 */
class CASCLocal : public CASC {
public:
	/**
	 * Create a new CASC source using a local installation.
	 * @param dir Installation path.
	 */
	explicit CASCLocal(const std::string& dir);

	/**
	 * Initialize local CASC source.
	 */
	void init();

	/**
	 * Obtain a file by it's fileDataID as a BLTEReader.
	 *
	 * In JS this overrides CASC.getFile() polymorphically (JS allows different return
	 * types). In C++, virtual getFile() returns std::string (encoding key), so we use a
	 * separate method name. Callers needing the decoded file should call getFileAsBLTE()
	 * directly on a CASCLocal instance rather than through the base CASC pointer.
	 *
	 * @param fileDataID
	 * @param partialDecryption
	 * @param suppressLog
	 * @param supportFallback
	 * @param forceFallback
	 * @param contentKey
	 */
	BLTEReader getFileAsBLTE(uint32_t fileDataID, bool partialDecryption = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "") override;

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
	BLTEStreamReader getFileStream(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "");

	/**
	 * Returns a list of available products in the installation.
	 * Format example: "PTR: World of Warcraft 8.3.0.32272"
	 */
	std::vector<ProductEntry> getProductList();

	/**
	 * Load the CASC interface with the given build.
	 * @param buildIndex
	 */
	void load(int buildIndex);

	/**
	 * Load the BuildConfig from the installation directory.
	 */
	void loadConfigs();

	/**
	 * Get config from disk with CDN fallback
	 */
	std::unordered_map<std::string, std::string> getConfigFileWithRemoteFallback(const std::string& key);

	/**
	 * Load and parse storage indexes from the local installation.
	 */
	void loadIndexes();

	/**
	 * Parse a local installation journal index for entries.
	 * @param file Path to the index.
	 */
	void parseIndex(const std::filesystem::path& file);

	/**
	 * Load and parse encoding from the local installation.
	 */
	void loadEncoding();

	/**
	 * Load and parse root table from local installation.
	 */
	void loadRoot();

	/**
	 * Initialize a remote CASC instance to download missing
	 * files needed during local initialization.
	 */
	void initializeRemoteCASC();

	/**
	 * Obtain a data file from the local archives.
	 * If not stored locally, file will be downloaded from a CDN.
	 * @param key
	 * @param forceFallback
	 */
	BufferWrapper getDataFileWithRemoteFallback(const std::string& key, bool forceFallback = false) override;

	/**
	 * Obtain a data file from the local archives.
	 * @param key
	 */
	BufferWrapper getDataFile(const std::string& key) override;

	/**
	 * Format a local path to a data archive.
	 * 67 -> <install>/Data/data/data.067
	 * @param id
	 */
	std::string formatDataPath(int32_t id);

	/**
	 * Format a local path to an archive index from the key.
	 * @param key
	 */
	std::string formatIndexPath(const std::string& key);

	/**
	 * Format a local path to a config file from the key.
	 * @param key
	 */
	std::string formatConfigPath(const std::string& key);

	/**
	 * Format a CDN key for use in local file reading.
	 * Path separators used by this method are platform specific.
	 * 49299eae4e3a195953764bb4adb3c91f -> 49/29/49299eae4e3a195953764bb4adb3c91f
	 * @param key
	 */
	std::string formatCDNKey(const std::string& key) override;

	/**
	 * ensure file is in cache (unwrapped from BLTE) and return path.
	 * @param encodingKey
	 * @param fileDataID
	 * @param suppressLog
	 * @returns path to cached file
	 */
	std::string _ensureFileInCache(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog) override;

	/**
	 * Get encoding info for a file by fileDataID for CDN streaming.
	 * @param fileDataID
	 * @returns FileEncodingInfo or std::nullopt
	 */
	std::optional<FileEncodingInfo> getFileEncodingInfo(uint32_t fileDataID) override;

	/**
	 * Get the current build ID.
	 * @returns build name string
	 */
	std::string getBuildName() override;

	/**
	 * Returns the build configuration key.
	 * @returns build key string
	 */
	std::string getBuildKey() override;

	// Async-equivalent API surface mirroring JS Promise methods.
	std::future<void> initAsync();
	std::future<BLTEReader> getFileAsBLTEAsync(uint32_t fileDataID, bool partialDecryption = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "");
	std::future<BLTEStreamReader> getFileStreamAsync(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "");
	std::future<void> loadAsync(int buildIndex);
	std::future<void> loadConfigsAsync();
	std::future<std::unordered_map<std::string, std::string>> getConfigFileWithRemoteFallbackAsync(const std::string& key);
	std::future<void> loadIndexesAsync();
	std::future<void> parseIndexAsync(const std::filesystem::path& file);
	std::future<void> loadEncodingAsync();
	std::future<void> loadRootAsync();
	std::future<void> initializeRemoteCASCAsync();
	std::future<BufferWrapper> getDataFileWithRemoteFallbackAsync(const std::string& key, bool forceFallback = false);
	std::future<BufferWrapper> getDataFileAsync(const std::string& key);
	std::future<std::string> ensureFileInCacheAsync(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog);
	std::future<std::optional<FileEncodingInfo>> getFileEncodingInfoAsync(uint32_t fileDataID);

	// Data members
	std::string dir;
	std::filesystem::path dataDir;
	std::filesystem::path storageDir;
	std::unordered_map<std::string, LocalIndexEntry> localIndexes;
	std::vector<std::unordered_map<std::string, std::string>> builds;
	std::unordered_map<std::string, std::string> build;
	std::unique_ptr<BuildCache> ownedCache;
	BuildCache* cache = nullptr;
	std::unique_ptr<CASCRemote> remote;
};

} // namespace casc

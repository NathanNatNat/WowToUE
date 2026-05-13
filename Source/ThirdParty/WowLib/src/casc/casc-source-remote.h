/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <mutex>
#include <future>

#include "casc-source.h"
#include "build-cache.h"
#include "blte-reader.h"
#include "../buffer.h"

namespace casc {

class BLTEStreamReader;

/**
 * Product entry from a remote CDN build.
 * JS equivalent: entries in this.builds array.
 */
struct ProductEntry {
	std::string label;
	int expansionId = 0;
	int buildIndex = 0;
};

/**
 * CASCRemote — CASC source using a Blizzard CDN.
 *
 * JS equivalent: class CASCRemote extends CASC — module.exports = CASCRemote
 */
class CASCRemote : public CASC {
public:
	/**
	 * Create a new CASC source using a Blizzard CDN.
	 * @param region Region tag (eu, us, etc).
	 */
	explicit CASCRemote(const std::string& region);

	/**
	 * Initialize remote CASC source.
	 */
	void init();

	/**
	 * Download the remote version config for a specific product.
	 * @param product
	 */
	std::vector<std::unordered_map<std::string, std::string>> getVersionConfig(const std::string& product);

	/**
	 * Download and parse a version config file.
	 * @param product
	 * @param file
	 */
	std::vector<std::unordered_map<std::string, std::string>> getConfig(const std::string& product, const std::string& file);

	/**
	 * Download and parse a CDN config file.
	 * Attempts multiple CDN hosts in order of ping speed if one fails.
	 * @param key
	 * @param cdnHosts Optional array of CDN hosts to try (in priority order)
	 */
	std::unordered_map<std::string, std::string> getCDNConfig(const std::string& key,
		const std::vector<std::string>& cdnHosts = {});

	/**
	 * Obtain a file by it's fileDataID.
	 * @param fileDataID
	 * @param partialDecrypt
	 * @param suppressLog
	 * @param supportFallback
	 * @param forceFallback
	 * @param contentKey
	 */
	// TODO: This could do with being an interface.
	BLTEReader getFileAsBLTE(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "") override;

	/**
	 * Get a streaming reader for a file by its fileDataID.
	 * @param fileDataID
	 * @param partialDecrypt
	 * @param suppressLog
	 * @param contentKey
	 * @returns BLTEStreamReader
	 */
	BLTEStreamReader getFileStream(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, const std::string& contentKey = "");

	/**
	 * Returns a list of available products on the remote CDN.
	 * Format example: "PTR: World of Warcraft 8.3.0.32272"
	 */
	std::vector<ProductEntry> getProductList();

	/**
	 * Preload requirements for reading remote files without initializing the
	 * entire instance. Used by local CASC install for CDN fallback.
	 * @param buildIndex
	 * @param cache Optional shared cache. If null, a new one is created.
	 */
	void preload(int buildIndex, BuildCache* cache = nullptr);

	/**
	 * Load the CASC interface with the given build.
	 * @param buildIndex
	 */
	void load(int buildIndex);

	/**
	 * Download and parse the encoding file.
	 */
	void loadEncoding();

	/**
	 * Download and parse the root file.
	 */
	void loadRoot();

	/**
	 * Download and parse archive files.
	 */
	void loadArchives();

	/**
	 * Download the CDN configuration and store the entry for our
	 * selected region.
	 */
	void loadServerConfig();

	/**
	 * Load and parse the contents of an archive index.
	 * Will use global cache and download if missing.
	 * @param key
	 */
	void parseArchiveIndex(const std::string& key);

	/**
	 * Download a data file from the CDN.
	 * @param file
	 * @returns BufferWrapper
	 */
	BufferWrapper getDataFile(const std::string& file) override;

	/**
	 * Download a partial chunk of a data file from the CDN.
	 * @param file
	 * @param ofs
	 * @param len
	 * @returns BufferWrapper
	 */
	BufferWrapper getDataFilePartial(const std::string& file, int64_t ofs, int64_t len);

	/**
	 * Download the CDNConfig and BuildConfig.
	 */
	void loadConfigs();

	/**
	 * Resolve the fastest CDN host for this region and server configuration.
	 */
	void resolveCDNHost();

	/**
	 * Format a CDN key for use in CDN requests.
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
	std::future<std::vector<std::unordered_map<std::string, std::string>>> getVersionConfigAsync(const std::string& product);
	std::future<std::vector<std::unordered_map<std::string, std::string>>> getConfigAsync(const std::string& product, const std::string& file);
	std::future<std::unordered_map<std::string, std::string>> getCDNConfigAsync(const std::string& key,
		const std::vector<std::string>& cdnHosts = {});
	std::future<BLTEReader> getFileAsBLTEAsync(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "");
	std::future<BLTEStreamReader> getFileStreamAsync(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, const std::string& contentKey = "");
	std::future<void> preloadAsync(int buildIndex, BuildCache* cache = nullptr);
	std::future<void> loadAsync(int buildIndex);
	std::future<void> loadEncodingAsync();
	std::future<void> loadRootAsync();
	std::future<void> loadArchivesAsync();
	std::future<void> loadServerConfigAsync();
	std::future<void> parseArchiveIndexAsync(const std::string& key);
	std::future<BufferWrapper> getDataFileAsync(const std::string& file);
	std::future<BufferWrapper> getDataFilePartialAsync(const std::string& file, int64_t ofs, int64_t len);
	std::future<void> loadConfigsAsync();
	std::future<void> resolveCDNHostAsync();
	std::future<std::string> ensureFileInCacheAsync(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog);
	std::future<std::optional<FileEncodingInfo>> getFileEncodingInfoAsync(uint32_t fileDataID);

	// Data members
	struct ArchiveEntry {
		std::string key;
		int32_t size;
		int32_t offset;
	};

	std::unordered_map<std::string, ArchiveEntry> archives;
	mutable std::mutex archivesMutex;
	std::string region;
	std::string host;
	std::vector<std::unordered_map<std::string, std::string>> builds;
	std::unordered_map<std::string, std::string> build;
	std::unordered_map<std::string, std::string> serverConfig;
	std::unique_ptr<BuildCache> ownedCache;
	BuildCache* cache = nullptr;
};

} // namespace casc

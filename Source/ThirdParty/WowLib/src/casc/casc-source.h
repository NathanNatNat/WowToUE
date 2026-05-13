/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <future>

#include "../buffer.h"
#include "blte-reader.h"
#include "install-manifest.h"
#include "listfile.h"

namespace casc {

constexpr uint16_t ENC_MAGIC = 0x4E45;
constexpr uint32_t ROOT_MAGIC = 0x4D465354;

/**
 * Encoding info for a file by fileDataID for CDN streaming.
 * JS equivalent: { enc: string, arc?: { key: string, ofs: number, len: number } }
 */
struct ArchiveInfo {
	std::string key;
	int32_t ofs;
	int32_t len;
};

struct FileEncodingInfo {
	std::string enc;
	std::optional<ArchiveInfo> arc;
};

/**
 * Root type entry — contentFlags + localeFlags pair.
 */
struct RootType {
	uint32_t contentFlags;
	uint32_t localeFlags;
};

/**
 * CASC — Base class for CASC data access.
 * Handles root/encoding file parsing and common file lookup.
 *
 * JS equivalent: class CASC — module.exports = CASC
 */
class CASC {
public:
	/**
	 * Construct a new CASC instance.
	 * @param isRemote True if this is a remote (CDN) source.
	 */
	explicit CASC(bool isRemote = false);

	virtual ~CASC();

	/**
	 * Provides an array of fileDataIDs that match the current locale.
	 * @returns Array of file data IDs.
	 */
	std::vector<uint32_t> getValidRootEntries();

	/**
	 * Retrieves the install manifest for this CASC instance.
	 * @returns InstallManifest
	 */
	InstallManifest getInstallManifest();

	/**
	 * Check if a file exists by its fileDataID.
	 * @param fileDataID
	 * @returns true if file exists for current locale.
	 */
	bool fileExists(uint32_t fileDataID);

	/**
	 * Obtain a file by it's fileDataID.
	 * Returns the encoding key for the file.
	 * @param fileDataID
	 */
	virtual std::string getFile(uint32_t fileDataID);

	/**
	 * Obtain a file by it's fileDataID as a BLTE reader.
	 * Sub-classes must override to provide JS-equivalent file reader behavior.
	 */
	virtual BLTEReader getFileAsBLTE(uint32_t fileDataID, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true,
		bool forceFallback = false, const std::string& contentKey = "") = 0;

	/**
	 * @param contentKey
	 * @returns encoding key string
	 */
	std::string getEncodingKeyForContentKey(const std::string& contentKey);

	/**
	 * Get encoding info for a file by fileDataID for CDN streaming.
	 * @param fileDataID
	 * @returns FileEncodingInfo or std::nullopt
	 */
	virtual std::optional<FileEncodingInfo> getFileEncodingInfo(uint32_t fileDataID);

	/**
	 * Obtain a file by a filename.
	 * fileName must exist in the loaded listfile.
	 * @param fileName
	 * @param partialDecrypt
	 * @param suppressLog
	 * @param supportFallback
	 * @param forceFallback
	 */
	BLTEReader getFileByName(const std::string& fileName, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true, bool forceFallback = false);

	/**
	 * get memory-mapped file by fileDataID.
	 * ensures file is in cache (unwrapped from BLTE), then returns bufferwrapper wrapping mmap.
	 * @param fileDataID
	 * @param suppressLog
	 * @returns BufferWrapper wrapper around memory-mapped file
	 */
	BufferWrapper getVirtualFileByID(uint32_t fileDataID, bool suppressLog = false);

	/**
	 * get memory-mapped file by filename.
	 * @param fileName
	 * @param suppressLog
	 * @returns BufferWrapper wrapper around memory-mapped file
	 */
	BufferWrapper getVirtualFileByName(const std::string& fileName, bool suppressLog = false);

	/**
	 * Prepare listfile data before loading.
	 * Ensures preloading is complete to avoid race conditions.
	 */
	void prepareListfile();

	/**
	 * prepare dbd manifest before loading.
	 * ensures preloading is complete.
	 */
	void prepareDBDManifest();

	/**
	 * Load the listfile for selected build.
	 * @param buildKey
	 */
	void loadListfile(const std::string& buildKey);

	/**
	 * Returns an array of model formats to display.
	 * @returns Array of extension filters.
	 */
	std::vector<listfile::ExtFilter> getModelFormats();

	/**
	 * Parse entries from a root file.
	 * @param data
	 * @param hash
	 * @returns Number of root entries.
	 */
	size_t parseRootFile(BufferWrapper data, const std::string& hash);

	/**
	 * Parse entries from an encoding file.
	 * @param data
	 * @param hash
	 */
	void parseEncodingFile(BufferWrapper data, const std::string& hash);

	/**
	 * Run any necessary clean-up once a CASC instance is no longer
	 * needed. At this point, the instance must be made eligible for GC.
	 */
	void cleanup();

	/**
	 * Format a CDN key for use in CDN requests.
	 * 49299eae4e3a195953764bb4adb3c91f -> 49/29/49299eae4e3a195953764bb4adb3c91f
	 * @param key
	 */
	virtual std::string formatCDNKey(const std::string& key);

	/**
	 * Download a data file from the CDN.
	 * Must be implemented by subclasses.
	 * @param file
	 * @returns BufferWrapper
	 */
	virtual BufferWrapper getDataFile(const std::string& file);

	/**
	 * Obtain a data file from local archives with remote fallback.
	 * Only used by CASCLocal. Default throws.
	 * @param key
	 * @param forceFallback
	 */
	virtual BufferWrapper getDataFileWithRemoteFallback(const std::string& key, bool forceFallback = false);

	/**
	 * ensure file is in cache (unwrapped from BLTE) and return path.
	 * Must be implemented by subclasses.
	 * @param encodingKey
	 * @param fileDataID
	 * @param suppressLog
	 * @returns path to cached file
	 */
	virtual std::string _ensureFileInCache(const std::string& encodingKey, uint32_t fileDataID, bool suppressLog);

	/**
	 * Get the current build ID.
	 * @returns build name string
	 */
	virtual std::string getBuildName() = 0;

	/**
	 * Returns the build configuration key.
	 * @returns build key string
	 */
	virtual std::string getBuildKey() = 0;

	// Async-equivalent API surface mirroring JS Promise methods.
	std::future<InstallManifest> getInstallManifestAsync();
	std::future<std::string> getFileAsync(uint32_t fileDataID);
	std::future<std::optional<FileEncodingInfo>> getFileEncodingInfoAsync(uint32_t fileDataID);
	std::future<BLTEReader> getFileByNameAsync(const std::string& fileName, bool partialDecrypt = false,
		bool suppressLog = false, bool supportFallback = true, bool forceFallback = false);
	std::future<BufferWrapper> getVirtualFileByIDAsync(uint32_t fileDataID, bool suppressLog = false);
	std::future<BufferWrapper> getVirtualFileByNameAsync(const std::string& fileName, bool suppressLog = false);
	std::future<void> prepareListfileAsync();
	std::future<void> prepareDBDManifestAsync();
	std::future<void> loadListfileAsync(const std::string& buildKey);
	std::future<size_t> parseRootFileAsync(BufferWrapper data, const std::string& hash);
	std::future<void> parseEncodingFileAsync(BufferWrapper data, const std::string& hash);

	// Thread-safety mutex for serialising CASC I/O from multiple async load tasks.
	mutable std::recursive_mutex io_mutex;

	// Data members
	std::unordered_map<std::string, int64_t> encodingSizes;
	std::unordered_map<std::string, std::string> encodingKeys;
	std::vector<RootType> rootTypes;
	std::unordered_map<uint32_t, std::unordered_map<size_t, std::string>> rootEntries;
	bool isRemote;
	uint32_t locale = 0;
	std::unordered_map<std::string, std::string> buildConfig;
	std::unordered_map<std::string, std::string> cdnConfig;

private:
	size_t unhookConfigId = 0;
};

} // namespace casc

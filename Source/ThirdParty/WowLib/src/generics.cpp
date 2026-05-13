/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "generics.h"
#include "buffer.h"
#include "constants.h"
#include "log.h"

#include <cmath>
#include <cstring>
#include <chrono>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <format>
#include <thread>
#include <future>
#include <functional>
#include <array>
#include <typeinfo>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <nlohmann/json.hpp>
#include <httplib.h>

#include <zlib.h>
#include <openssl/evp.h>

namespace generics {

namespace {


constexpr const char* JEDEC[] = {
	"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
};

struct ParsedURL {
	std::string scheme;
	std::string host;
	int port;
	std::string path;
};

ParsedURL parseURL(const std::string& url) {
	ParsedURL result;

	size_t schemeEnd = url.find("://");
	if (schemeEnd == std::string::npos)
		throw std::runtime_error("Invalid URL: " + url);

	result.scheme = url.substr(0, schemeEnd);
	size_t hostStart = schemeEnd + 3;
	size_t pathStart = url.find('/', hostStart);
	std::string hostPort;

	if (pathStart == std::string::npos) {
		hostPort = url.substr(hostStart);
		result.path = "/";
	} else {
		hostPort = url.substr(hostStart, pathStart - hostStart);
		result.path = url.substr(pathStart);
	}

	size_t colonPos = hostPort.find(':');
	if (colonPos != std::string::npos) {
		result.host = hostPort.substr(0, colonPos);
		result.port = std::stoi(hostPort.substr(colonPos + 1));
	} else {
		result.host = hostPort;
		result.port = (result.scheme == "https") ? 443 : 80;
	}

	return result;
}

HttpResponse doHttpGet(const std::string& url,
                      int64_t partialOfs = -1,
                      int64_t partialLen = -1,
                      bool followLocation = true,
                      int connectionTimeoutSeconds = 30,
                      int readTimeoutSeconds = 30,
                      int totalTimeoutSeconds = 30) {
	auto parsed = parseURL(url);

	httplib::Headers headers;
	headers.emplace("User-Agent", std::string(constants::USER_AGENT()));

	if (partialOfs > -1 && partialLen > -1) {
		headers.emplace("Range", std::format("bytes={}-{}", partialOfs, partialOfs + partialLen - 1));
	}

	auto performGet = [parsed, headers, followLocation, connectionTimeoutSeconds, readTimeoutSeconds]() -> httplib::Result {
		if (parsed.scheme == "https") {
			httplib::SSLClient cli(parsed.host, parsed.port);
			cli.set_connection_timeout(connectionTimeoutSeconds);
			cli.set_read_timeout(readTimeoutSeconds);
			cli.set_follow_location(followLocation);
			return cli.Get(parsed.path, headers);
		}

		httplib::Client cli(parsed.host, parsed.port);
		cli.set_connection_timeout(connectionTimeoutSeconds);
		cli.set_read_timeout(readTimeoutSeconds);
		cli.set_follow_location(followLocation);
		return cli.Get(parsed.path, headers);
	};

	httplib::Result res;
	if (totalTimeoutSeconds > 0) {
		auto promise = std::make_shared<std::promise<httplib::Result>>();
		auto future = promise->get_future();
		std::thread worker([promise, performGet = std::move(performGet)]() mutable {
			try {
				promise->set_value(performGet());
			} catch (...) {
				promise->set_exception(std::current_exception());
			}
		});

		if (future.wait_for(std::chrono::seconds(totalTimeoutSeconds)) == std::future_status::ready) {
			worker.join();
			res = future.get();
		} else {
			worker.detach();
			throw std::runtime_error("The operation was aborted due to timeout");
		}
	} else {
		res = performGet();
	}

	if (!res)
		throw std::runtime_error(std::format("HTTP request failed for {}: {}", url, httplib::to_string(res.error())));

	HttpResponse response;
	response.status = res->status;
	response.statusText = res->reason;
	response.ok = (res->status >= 200 && res->status < 300);
	for (const auto& [key, value] : res->headers)
		response.headers[key] = value;
	const auto& body = res->body;
	response.body = std::vector<uint8_t>(body.begin(), body.end());
	return response;
}

HttpResponse doHttpGetRaw(const std::string& url,
                          int64_t partialOfs = -1,
                          int64_t partialLen = -1) {
	auto parsed = parseURL(url);

	httplib::Headers headers;
	headers.emplace("User-Agent", std::string(constants::USER_AGENT()));

	if (partialOfs > -1 && partialLen > -1) {
		headers.emplace("Range", std::format("bytes={}-{}", partialOfs, partialOfs + partialLen - 1));
	}

	int64_t totalSize = 0;
	int64_t downloaded = 0;
	int last_logged_pct = 0;
	std::vector<uint8_t> bodyData;

	httplib::ContentReceiver contentReceiver = [&](const char* data, size_t len) -> bool {
		bodyData.insert(bodyData.end(), data, data + len);
		downloaded = static_cast<int64_t>(bodyData.size());

		if (totalSize > 0) {
			int pct = static_cast<int>((static_cast<double>(downloaded) / totalSize) * 100);
			int pct_threshold = (pct / 25) * 25;

			if (pct_threshold > last_logged_pct && pct_threshold < 100) {
				logging::write(std::format("Download progress: {}/{} bytes ({}%)", downloaded, totalSize, pct));
				last_logged_pct = pct_threshold;
			}
		}
		return true;
	};

	httplib::ResponseHandler responseHandler = [&](const httplib::Response& res) -> bool {
		auto it = res.headers.find("Content-Length");
		if (it != res.headers.end()) {
			totalSize = std::stoll(it->second);
			logging::write(std::format("Starting download: {} bytes expected", totalSize));
			bodyData.reserve(static_cast<size_t>(totalSize * 1.5));
		}
		return true;
	};

	httplib::Result res;

	if (parsed.scheme == "https") {
		httplib::SSLClient cli(parsed.host, parsed.port);
		cli.set_connection_timeout(60);
		cli.set_read_timeout(300);
		cli.set_follow_location(false);
		res = cli.Get(parsed.path, headers, responseHandler, contentReceiver);
	} else {
		httplib::Client cli(parsed.host, parsed.port);
		cli.set_connection_timeout(60);
		cli.set_read_timeout(300);
		cli.set_follow_location(false);
		res = cli.Get(parsed.path, headers, responseHandler, contentReceiver);
	}

	if (!res)
		throw std::runtime_error(std::format("HTTP request failed for {}: {}", url, httplib::to_string(res.error())));

	HttpResponse response;
	response.status = res->status;
	response.statusText = res->reason;
	response.ok = (res->status >= 200 && res->status < 300);
	for (const auto& [key, value] : res->headers)
		response.headers[key] = value;
	response.body = std::move(bodyData);
	return response;
}

std::vector<uint8_t> inflateData(const std::vector<uint8_t>& input) {
	z_stream strm{};
	strm.next_in = const_cast<uint8_t*>(input.data());
	strm.avail_in = static_cast<uInt>(input.size());

	if (inflateInit(&strm) != Z_OK)
		throw std::runtime_error("Failed to initialize zlib inflate");

	std::vector<uint8_t> output;
	uint8_t chunk[16384];

	int ret;
	do {
		strm.next_out = chunk;
		strm.avail_out = sizeof(chunk);
		ret = inflate(&strm, Z_NO_FLUSH);

		if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
			inflateEnd(&strm);
			throw std::runtime_error("zlib inflate error");
		}

		size_t have = sizeof(chunk) - strm.avail_out;
		output.insert(output.end(), chunk, chunk + have);
	} while (ret != Z_STREAM_END);

	inflateEnd(&strm);
	return output;
}

std::string computeFileHash(const std::filesystem::path& file,
                            std::string_view method,
                            std::string_view encoding) {
	std::ifstream ifs(file, std::ios::binary);
	if (!ifs.is_open())
		throw std::runtime_error("Failed to open file for hashing: " + file.string());

	const auto nameToMD = [](std::string_view name) -> const EVP_MD* {
		if (name == "md5")    return EVP_md5();
		if (name == "sha1")   return EVP_sha1();
		if (name == "sha224") return EVP_sha224();
		if (name == "sha256") return EVP_sha256();
		if (name == "sha384") return EVP_sha384();
		if (name == "sha512") return EVP_sha512();
		return nullptr;
	};

	const EVP_MD* md = nameToMD(method);
	if (!md)
		throw std::runtime_error("computeFileHash: unsupported algorithm '" + std::string(method) + "'");

	EVP_MD_CTX* ctx = EVP_MD_CTX_new();
	if (!ctx)
		throw std::runtime_error("computeFileHash: EVP_MD_CTX_new failed");
	if (EVP_DigestInit_ex(ctx, md, nullptr) != 1) {
		EVP_MD_CTX_free(ctx);
		throw std::runtime_error("computeFileHash: failed to initialize hash");
	}

	constexpr size_t CHUNK = 65536;
	std::array<char, CHUNK> buf;
	while (ifs.read(buf.data(), CHUNK) || ifs.gcount() > 0) {
		auto n = static_cast<size_t>(ifs.gcount());
		if (EVP_DigestUpdate(ctx, buf.data(), n) != 1) {
			EVP_MD_CTX_free(ctx);
			throw std::runtime_error("computeFileHash: hash update failed");
		}
		if (ifs.eof()) break;
	}

	std::vector<uint8_t> digest(EVP_MD_size(md));
	unsigned int digest_len = 0;
	if (EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) != 1) {
		EVP_MD_CTX_free(ctx);
		throw std::runtime_error("computeFileHash: hash finalization failed");
	}
	EVP_MD_CTX_free(ctx);
	digest.resize(digest_len);

	if (encoding == "hex") {
		constexpr char hex_chars[] = "0123456789abcdef";
		std::string result;
		result.reserve(digest.size() * 2);
		for (uint8_t b : digest) {
			result += hex_chars[b >> 4];
			result += hex_chars[b & 0xf];
		}
		return result;
	}
	if (encoding == "base64") {
		BufferWrapper tmp(std::move(digest));
		return tmp.calculateHash(method, "base64");
	}
	throw std::runtime_error("computeFileHash: unsupported encoding '" + std::string(encoding) + "'");
}

} // anonymous namespace

std::string HttpResponse::text() const {
	return std::string(body.begin(), body.end());
}

nlohmann::json HttpResponse::json() const {
	std::string_view sv(reinterpret_cast<const char*>(body.data()), body.size());
	auto parsed = nlohmann::json::parse(sv, nullptr, false);
	if (parsed.is_discarded())
		throw std::runtime_error("Unable to parse response body as JSON");
	return parsed;
}

/**
 * Async wrapper for HTTP GET.
 * Supports URL fallback chains (tries each URL in order).
 * @param url Single URL.
 * @returns Fetch-style response object.
 */
HttpResponse get(const std::string& url) {
	return get(std::vector<std::string>{url});
}

/**
 * Async wrapper for HTTP GET.
 * Supports URL fallback chains (tries each URL in order).
 * @param urls List of fallback URLs.
 * @returns Fetch-style response object.
 */
HttpResponse get(const std::vector<std::string>& urls) {
	size_t index = 1;
	HttpResponse res;

	for (const auto& url : urls) {
		logging::write(std::format("get -> [{}/{}]: {}", index, urls.size(), url));

		try {
			res = doHttpGet(url);
			logging::write(std::format("get -> [{}][{}] {}", index, res.status, url));
			index++;

			if (res.ok)
				break;
		} catch (const std::exception& error) {
			logging::write(std::format("fetch failed {}: {}", url, error.what()));
			index++;
			if (index > urls.size())
				throw;
		}
	}

	return res;
}

/**
 * Dispatch a handler for an array of items with a limit to how
 * many can be resolving at once.
 *
 * @param items    Each one is passed to the handler.
 * @param handler  Called for each item.
 * @param limit    This many will be resolving at any given time.
 */
template <typename T>
void queue(const std::vector<T>& items,
           const std::function<void(const T&)>& handler,
           size_t limit) {
	size_t maxConcurrent = limit + 1;
	size_t index = 0;
	size_t complete = 0;
	std::vector<std::future<void>> futures;

	while (complete < items.size()) {
		while (futures.size() < maxConcurrent && index < items.size()) {
			futures.push_back(std::async(std::launch::async, [&handler, item = items[index]]() {
				handler(item);
			}));
			index++;
		}

		if (!futures.empty()) {
			bool found = false;
			while (!found) {
				for (size_t i = 0; i < futures.size(); ++i) {
					if (futures[i].wait_for(std::chrono::milliseconds(1)) == std::future_status::ready) {
						futures[i].get();
						futures.erase(futures.begin() + static_cast<std::ptrdiff_t>(i));
						complete++;
						found = true;
						break;
					}
				}
				if (!found) {
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
				}
			}
		}
	}
}

/**
 * Ping a URL and measure the response time in milliseconds.
 * Not perfectly accurate, but good enough for our purposes.
 * Throws on error or HTTP code other than 200.
 * @param url URL to ping.
 */
int64_t ping(const std::string& url) {
	auto pingStart = std::chrono::steady_clock::now();

	get(url);

	auto elapsed = std::chrono::steady_clock::now() - pingStart;
	return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

/**
 * Attempt to parse JSON, returning std::nullopt on failure.
 * Inline function to keep code paths clean of unnecessary try/catch blocks.
 * @param data JSON string.
 */
std::optional<nlohmann::json> parseJSON(std::string_view data) {
	try {
		return nlohmann::json::parse(data);
	} catch (const nlohmann::json::exception&) {
		return std::nullopt;
	}
}

/**
 * Obtain JSON from a remote end-point.
 * @param url URL to fetch JSON from.
 */
nlohmann::json getJSON(const std::string& url) {
	auto response = get(url);
	if (!response.ok)
		throw std::runtime_error(std::format("Unable to request JSON from end-point. HTTP {} {}", response.status, response.statusText));

	return response.json();
}

/**
 * Read a JSON file from disk, returning std::nullopt on error.
 * Provides basic pruning for comments (lines starting with //) with ignoreComments.
 * @param file           Path to the JSON file.
 * @param ignoreComments If true, strips comment lines before parsing.
 */
std::optional<nlohmann::json> readJSON(const std::filesystem::path& file, bool ignoreComments) {
	try {
		std::ifstream ifs(file);
		if (!ifs.is_open()) {
			std::error_code ec;
			if (std::filesystem::exists(file, ec) && !ec)
				throw std::runtime_error("Permission denied: " + file.string());
			return std::nullopt;
		}

		std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		if (ignoreComments) {
			std::istringstream stream(raw);
			std::string line;
			std::string filtered;

			while (std::getline(stream, line)) {
				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				if (!line.starts_with("//"))
					filtered += line + '\n';
			}

			return nlohmann::json::parse(filtered);
		}

		return nlohmann::json::parse(raw);
	} catch (const std::runtime_error&) {
		throw;
	} catch (const std::exception&) {
		return std::nullopt;
	}
}

static std::vector<uint8_t> requestData(const std::string& url, int64_t partialOfs, int64_t partialLen) {
	logging::write(std::format("Requesting data from {} (offset: {}, length: {})", url, partialOfs, partialLen));

	auto response = doHttpGetRaw(url, partialOfs, partialLen);

	if (response.status == 301 || response.status == 302) {
		auto locationIt = response.headers.find("Location");
		if (locationIt == response.headers.end())
			locationIt = response.headers.find("location");
		if (locationIt == response.headers.end())
			throw std::runtime_error(std::format("Status Code: {}", response.status));

		logging::write("Got redirect to " + locationIt->second);
		return requestData(locationIt->second, partialOfs, partialLen);
	}

	if (response.status < 200 || response.status > 302)
		throw std::runtime_error(std::format("Status Code: {}", response.status));

	logging::write(std::format("Download complete: {} bytes received", response.body.size()));
	return std::move(response.body);
}

/**
 * Download a file (optionally to a local file).
 * Zlib inflation will be applied if deflate is true.
 * Data is always returned even if `out` is provided.
 * @param url        Remote URL.
 * @param out        Optional file path to save downloaded data.
 * @param partialOfs Partial content start offset (-1 to disable).
 * @param partialLen Partial content length (-1 to disable).
 * @param deflate    If true, will inflate (decompress) the data.
 */
BufferWrapper downloadFile(const std::string& url, const std::string& out,
                           int64_t partialOfs, int64_t partialLen, bool doDeflate, int mode) {
	return downloadFile(std::vector<std::string>{url}, out, partialOfs, partialLen, doDeflate, mode);
}

/**
 * Download a file (optionally to a local file).
 * Supports URL fallback chains.
 * @param urls       List of fallback URLs.
 * @param out        Optional file path to save downloaded data.
 * @param partialOfs Partial content start offset (-1 to disable).
 * @param partialLen Partial content length (-1 to disable).
 * @param deflate    If true, will inflate (decompress) the data.
 */
BufferWrapper downloadFile(const std::vector<std::string>& urls, const std::string& out,
                           int64_t partialOfs, int64_t partialLen, bool doDeflate, int mode) {
	mode &= 07777;
	constexpr int MAX_RETRIES = 3;

	for (const auto& currentUrl : urls) {
		if (currentUrl.empty())
			continue;

		for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
			try {
				if (attempt > 1)
					logging::write(std::format("downloadFile retry {}/{} -> {}", attempt, MAX_RETRIES, currentUrl));
				else
					logging::write(std::format("downloadFile -> {}", currentUrl));

				auto data = requestData(currentUrl, partialOfs, partialLen);

				if (doDeflate)
					data = inflateData(data);

				BufferWrapper wrapped(std::move(data));

				if (!out.empty()) {
					createDirectory(std::filesystem::path(out).parent_path());
					wrapped.writeToFile(out);
					std::filesystem::permissions(out, static_cast<std::filesystem::perms>(mode));
				}

				return wrapped;
			} catch (const std::exception& error) {
				logging::write(std::format("Failed to download from {}: {}", currentUrl, error.what()));
				logging::write(std::format("Error details: type={} what={}", typeid(error).name(), error.what()));

				if (attempt < MAX_RETRIES)
					logging::write(std::format("Will retry ({}/{})", attempt + 1, MAX_RETRIES));
			}
		}
	}

	throw std::runtime_error("All download attempts failed.");
}

/**
 * Create all directories in a given path if they do not exist.
 * @param dir Directory path.
 */
void createDirectory(const std::filesystem::path& dir) {
	std::filesystem::create_directories(dir);
}

/**
 * Returns after a redraw.
 */
void redraw() {
	std::this_thread::yield();
	std::this_thread::yield();
}

/**
 * Format a number (bytes) to a displayable file size.
 * Simplified version of filesize.js using JEDEC units.
 * @param input Number of bytes.
 */
std::string filesize(double input) {
	if (std::isnan(input))
		return "NaN";

	const bool isNegative = input < 0;

	// Flipping a negative number to determining the size.
	if (isNegative)
		input = -input;

	// Determining the exponent.
	int exponent = (input > 0) ? static_cast<int>(std::floor(std::log(input) / std::log(1024.0))) : 0;
	if (exponent < 0)
		exponent = 0;

	// Exceeding supported length, time to reduce & multiply.
	if (exponent > 8)
		exponent = 8;

	std::string valueStr;
	std::string unit;

	// Zero is now a special case because bytes divide by 1.
	if (input == 0) {
		valueStr = "0";
		unit = JEDEC[exponent];
	} else {
		double val = input / std::pow(2.0, exponent * 10);

		if (exponent > 0) {
			std::ostringstream oss;
			oss << std::fixed << std::setprecision(2) << val;
			valueStr = oss.str();

			if (valueStr.find('.') != std::string::npos) {
				size_t lastNonZero = valueStr.find_last_not_of('0');
				if (lastNonZero != std::string::npos && valueStr[lastNonZero] == '.')
					valueStr.erase(lastNonZero);
				else
					valueStr.erase(lastNonZero + 1);
			}
		} else {
			std::ostringstream oss;
			oss << std::fixed << std::setprecision(0) << val;
			valueStr = oss.str();
		}

		double parsed = std::stod(valueStr);
		if (parsed == 1024.0 && exponent < 8) {
			valueStr = "1";
			exponent++;
		}

		unit = JEDEC[exponent];
	}

	// Decorating a 'diff'.
	if (isNegative)
		valueStr = "-" + valueStr;

	return valueStr + " " + unit;
}

/**
 * Calculate the hash of a file.
 * @param file     Path to the file to hash.
 * @param method   Hashing method (e.g. "md5").
 * @param encoding Output encoding (e.g. "hex").
 */
std::string getFileHash(const std::filesystem::path& file, std::string_view method, std::string_view encoding) {
	return computeFileHash(file, method, encoding);
}

/**
 * Wrapper for checking if a file exists.
 * @param file Path to the file.
 */
bool fileExists(const std::filesystem::path& file) {
	try {
		return std::filesystem::exists(file);
	} catch (const std::exception&) {
		return false;
	}
}

/**
 * Check if a directory exists and is writable.
 * @param dir Path to the directory.
 */
bool directoryIsWritable(const std::filesystem::path& dir) {
	try {
		if (!std::filesystem::is_directory(dir))
			return false;

#ifdef _WIN32
		auto testPath = dir / ".wow_export_cpp_write_test";
		std::ofstream ofs(testPath, std::ios::out | std::ios::trunc);
		if (!ofs.is_open())
			return false;
		ofs.close();
		std::filesystem::remove(testPath);
		return true;
#else
		return access(dir.c_str(), W_OK) == 0;
#endif
	} catch (const std::exception&) {
		return false;
	}
}

/**
 * Read a portion of a file.
 * @param file   Path of the file.
 * @param offset Offset to start reading from.
 * @param length Total bytes to read.
 */
BufferWrapper readFile(const std::filesystem::path& file, size_t offset, size_t length) {
	std::ifstream fd(file, std::ios::binary);
	if (!fd.is_open())
		throw std::runtime_error("Failed to open file: " + file.string());

	BufferWrapper buf = BufferWrapper::alloc(length);

	fd.seekg(static_cast<std::streamoff>(offset));
	fd.read(reinterpret_cast<char*>(buf.raw().data()), static_cast<std::streamsize>(length));
	fd.close();

	return buf;
}

/**
 * Recursively delete a directory and everything inside of it.
 * Returns the total size of all files deleted.
 * @param dir Directory to delete.
 */
uintmax_t deleteDirectory(const std::filesystem::path& dir) {
	uintmax_t deleteSize = 0;
	try {
		for (const auto& entry : std::filesystem::directory_iterator(dir)) {
			if (entry.is_directory()) {
				deleteSize += deleteDirectory(entry.path());
			} else {
				deleteSize += entry.file_size();
				std::filesystem::remove(entry.path());
			}
		}

		std::filesystem::remove(dir);
	} catch (const std::exception&) {
		// Something failed to delete.
	}

	return deleteSize;
}

/**
 * Process work in non-blocking batches.
 * Allows large amounts of work to be done without freezing the UI.
 *
 * @param name      Name for logging purposes.
 * @param work      Array of items to process.
 * @param processor Function called for each item: (item, index) => void
 * @param batchSize Items to process per batch.
 */
template <typename T>
void batchWork(std::string_view name,
               const std::vector<T>& work,
               const std::function<void(const T&, size_t)>& processor,
               size_t batchSize) {
	size_t index = 0;
	size_t total = work.size();
	auto startTime = std::chrono::steady_clock::now();
	int lastProgressPercent = 0;

	logging::write(std::format("Starting batch work \"{}\" with {} items...", name, total));

	while (index < total) {
		size_t endIndex = std::min(index + batchSize, total);

		for (size_t i = index; i < endIndex; i++)
			processor(work[i], i);

		index = endIndex;

		int progressPercent = static_cast<int>((static_cast<double>(index) / total) * 100);
		if (progressPercent >= lastProgressPercent + 10 && progressPercent < 100) {
			logging::write(std::format("Batch work \"{}\" progress: {}% ({}/{})", name, progressPercent, index, total));
			lastProgressPercent = progressPercent;
		}

		if (index < total)
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
	}

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - startTime).count();
	logging::write(std::format("Batch work \"{}\" completed in {}ms ({} items)", name, duration, total));
}

/**
 * Return a formatted representation of seconds.
 * Example: 26 will return "00:26"
 * @param seconds Number of seconds.
 */
std::string formatPlaybackSeconds(double seconds) {
	if (std::isnan(seconds))
		return "00:00";

	int mins = static_cast<int>(std::floor(seconds / 60.0));
	int secs = static_cast<int>(std::round(std::fmod(seconds, 60.0)));

	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(2) << mins
	    << ':' << std::setfill('0') << std::setw(2) << secs;
	return oss.str();
}

template void batchWork<std::string>(std::string_view, const std::vector<std::string>&,
	const std::function<void(const std::string&, size_t)>&, size_t);

template void batchWork<std::pair<uint32_t, std::string>>(std::string_view,
	const std::vector<std::pair<uint32_t, std::string>>&,
	const std::function<void(const std::pair<uint32_t, std::string>&, size_t)>&, size_t);

template void queue<std::string>(const std::vector<std::string>&,
	const std::function<void(const std::string&)>&, size_t);

} // namespace generics

/*!
 * Blob.js Polyfill for wow.export
 * Adapted from: https://github.com/eligrey/Blob.js by Kruithne <kruithne@gmail.com>
 * By Eli Grey, https://eligrey.com
 * By Jimmy Wärting, https://github.com/jimmywarting
 * License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <functional>
#include <optional>

class BlobPolyfill;
class BlobReadableStream;

/**
 * Represents a part of a Blob, enabling implicit construction
 * from byte data, strings, or other blobs.
 *
 * JS equivalent: element in the chunks array passed to the BlobPolyfill
 * constructor. In JS, each element can be a string, typed array,
 * ArrayBuffer, DataView, or another Blob — the type is determined
 * at runtime. In C++, the type is resolved at compile time through
 * overloaded constructors.
 */
struct BlobPart {
	std::vector<uint8_t> bytes;

	BlobPart(std::span<const uint8_t> data);
	BlobPart(const std::vector<uint8_t>& data);
	BlobPart(std::vector<uint8_t>&& data);
	BlobPart(std::string_view str);
	BlobPart(const BlobPolyfill& blob);
};

/**
 * Options for BlobPolyfill construction.
 * JS equivalent: { type: string } options parameter.
 */
struct BlobOptions {
	std::string type;
};

/**
 * BlobPolyfill class — a pure-data blob container.
 *
 * In the JS original, this was a polyfill for the browser Blob API
 * in environments where native Blob isn't available (NW.js).
 *
 * JS equivalent: module.exports.BlobPolyfill
 */
class BlobPolyfill {
public:
	/**
	 * Default constructor — creates an empty blob.
	 */
	BlobPolyfill();

	/**
	 * Construct a blob from parts with optional MIME type.
	 * JS equivalent: new BlobPolyfill(chunks, opts)
	 *
	 * Each BlobPart is concatenated in order to form the blob's data.
	 * The type is sanitized: if it contains non-printable-ASCII characters
	 * (outside 0x20-0x7E), it's cleared; otherwise it's lowercased.
	 *
	 * @param parts Vector of BlobParts (byte data, strings, or other blobs).
	 * @param opts Options with MIME type.
	 */
	explicit BlobPolyfill(std::vector<BlobPart> parts, BlobOptions opts = {});

	/**
	 * Returns the underlying buffer.
	 * JS equivalent: BlobPolyfill.prototype.arrayBuffer() — returns Promise<ArrayBuffer>.
	 */
	const std::vector<uint8_t>& arrayBuffer() const;

	/**
	 * Returns the blob content decoded as a UTF-8 string.
	 * JS equivalent: BlobPolyfill.prototype.text() — returns Promise<string>.
	 */
	std::string text() const;

	/**
	 * Returns a new blob containing a subset of this blob's data.
	 * JS equivalent: BlobPolyfill.prototype.slice(start, end, type)
	 *
	 * @param start Start byte offset (default 0).
	 * @param end End byte offset (default: blob size). Uses std::optional so
	 *            absent (nullopt) means "use full length"; explicit 0 is used
	 *            as-is (returns an empty blob for slice(x, 0) when x >= 0).
	 * @param type MIME type for the new blob (default: empty string).
	 */
	BlobPolyfill slice(std::size_t start = 0,
	                   std::optional<std::size_t> end = std::nullopt,
	                   const std::string& type = "") const;

	/**
	 * Returns "[object Blob]".
	 * JS equivalent: BlobPolyfill.prototype.toString()
	 */
	std::string toString() const;

	/**
	 * Iterates over the blob's data in 512KB chunks.
	 * JS equivalent: BlobPolyfill.prototype.stream() — returns a ReadableStream.
	 * as a span. The chunk size (524288 bytes) matches the JS original.
	 *
	 * @param callback Called once per chunk with a span of the chunk data.
	 */
	BlobReadableStream stream() const;
	void stream(std::function<void(std::span<const uint8_t>)> callback) const;

	/**
	 * Size of the blob in bytes.
	 * JS equivalent: BlobPolyfill.size property.
	 */
	std::size_t size() const;

	/**
	 * MIME type of the blob.
	 * JS equivalent: BlobPolyfill.type property.
	 */
	const std::string& type() const;

	/**
	 * Static flag indicating this is a polyfill implementation.
	 * JS equivalent: BlobPolyfill.isPolyfill = true
	 */
	static constexpr bool isPolyfill = true;

private:
	std::vector<uint8_t> _buffer;
	std::string _type;
};

/**
 * Lazy pull-based blob stream.
 * JS equivalent: ReadableStream returned by BlobPolyfill.stream().
 */
class BlobReadableStream {
public:
	explicit BlobReadableStream(const BlobPolyfill* blob);
	std::optional<std::vector<uint8_t>> pull();
	bool closed() const;

private:
	const BlobPolyfill* _blob = nullptr;
	std::size_t _position = 0;
};

/**
 * URLPolyfill class — creates/revokes data URLs from BlobPolyfill objects.
 *
 * In JS, this wraps native URL API with Blob polyfill support.
 *
 * JS equivalent: module.exports.URLPolyfill
 */
class URLPolyfill {
public:
	/**
	 * Create a data URL from a blob.
	 * JS equivalent: URLPolyfill.createObjectURL(blob)
	 *
	 * Returns a data URL in the format: data:<type>;base64,<encoded_data>
	 *
	 * @param blob The blob to encode as a data URL.
	 * @return Base64 data URL string.
	 */
	static std::string createObjectURL(const BlobPolyfill& blob);
	static std::string createObjectURL(std::span<const uint8_t> bytes, std::string_view type = "application/octet-stream");
	static std::optional<BlobPolyfill> resolveObjectURL(const std::string& url);

	/**
	 * Revoke a previously created URL.
	 * For data URLs (which is what createObjectURL produces), this is a no-op.
	 * JS equivalent: URLPolyfill.revokeObjectURL(url)
	 *
	 * @param url The URL to revoke.
	 */
	static void revokeObjectURL(const std::string& url);
};

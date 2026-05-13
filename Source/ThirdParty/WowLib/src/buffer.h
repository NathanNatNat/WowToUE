/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <optional>
#include <filesystem>
#include <variant>
#include <memory>

#include <nlohmann/json_fwd.hpp>

/**
 * This class is a wrapper for a byte buffer which provides a streamlined
 * interface for reading/writing data. Only required features have been
 * implemented.
 *
 * JS equivalent: class BufferWrapper — module.exports = BufferWrapper
 */
class BufferWrapper {
public:
	struct DecodedAudioData {
		std::vector<float> samples;
		uint32_t channels = 0;
		uint32_t sampleRate = 0;
	};

	// -----------------------------------------------------------------------
	// Static factory methods
	// -----------------------------------------------------------------------

	/**
	 * Alloc a buffer with the given length and return it wrapped.
	 * @param length Initial capacity of the internal buffer.
	 * @param secure If true, buffer will be zeroed for security.
	 */
	static BufferWrapper alloc(size_t length, bool secure = false);

	/**
	 * Create a buffer from a source.
	 */
	static BufferWrapper from(std::span<const uint8_t> source);
	static BufferWrapper from(std::vector<uint8_t>&& source);

	/**
	 * Create a buffer from a base64-encoded string.
	 */
	static BufferWrapper fromBase64(std::string_view source);

	/**
	 * Concatenate an array of buffers into a single buffer.
	 */
	static BufferWrapper concat(const std::vector<BufferWrapper>& buffers);

	/**
	 * C++ equivalent of JS fromCanvas(). The JS method accepts an HTMLCanvasElement or
	 * OffscreenCanvas and internally calls canvas.getImageData() to extract pixels.
	 * C++ has no browser canvas API, so callers must extract RGBA pixel data first and
	 * pass it directly.
	 * @param rgba   Pointer to RGBA pixel data (4 bytes per pixel).
	 * @param width  Image width in pixels.
	 * @param height Image height in pixels.
	 * @param mimeType Target format (e.g., "image/png", "image/webp").
	 * @param quality Quality setting (1-100), 100 = lossless for WebP.
	 */
	static BufferWrapper fromPixelData(const uint8_t* rgba, int width, int height,
	                                   std::string_view mimeType, int quality = 90);
	static BufferWrapper fromCanvas(const uint8_t* rgba, int width, int height,
	                                std::string_view mimeType, int quality = 90);

	// JS readFile() is async and returns Promise<BufferWrapper>. C++ is synchronous and blocks the calling thread.
	static BufferWrapper readFile(const std::filesystem::path& file);

	/**
	 * Create BufferWrapper from mmap instance.
	 * Wraps mmap data without copying.
	 * @param mmapData Pointer to memory-mapped data.
	 * @param size     Size of the mapped region in bytes.
	 */
	static BufferWrapper fromMmap(void* mmapData, size_t size);

	// -----------------------------------------------------------------------
	// Constructors / assignment
	// -----------------------------------------------------------------------

	/** Default constructor — creates an empty buffer. */
	BufferWrapper();

	/** Construct a new BufferWrapper from an existing buffer. */
	explicit BufferWrapper(std::vector<uint8_t> buf);

	/** Construct with an initial offset. */
	BufferWrapper(std::vector<uint8_t> buf, size_t offset);

	/** Move constructor. */
	BufferWrapper(BufferWrapper&& other) noexcept;

	/** Move assignment. */
	BufferWrapper& operator=(BufferWrapper&& other) noexcept;

	/** Copy constructor. */
	BufferWrapper(const BufferWrapper& other);

	/** Copy assignment. */
	BufferWrapper& operator=(const BufferWrapper& other);

	// TODO 69: The JS BufferWrapper has no virtual methods or inheritance.
	// In C++, _checkBounds is virtual and the destructor is virtual so that
	// BLTEReader can subclass BufferWrapper and override _checkBounds to lazily
	// decompress blocks on demand. This is a C++-specific design choice with
	// no direct JS equivalent — JS uses prototype chain dynamism instead.
	// The vtable overhead is negligible for the buffer sizes involved.
	virtual ~BufferWrapper();

	// -----------------------------------------------------------------------
	// Property getters (JS `get` accessors)
	// -----------------------------------------------------------------------

	/** Get the full capacity of the buffer. */
	size_t byteLength() const;

	/** Get the amount of remaining bytes until the end of the buffer. */
	size_t remainingBytes() const;

	/** Get the current offset within the buffer. */
	size_t offset() const;

	/** Get the raw buffer wrapped by this instance. */
	std::span<const uint8_t> raw() const;
	std::span<uint8_t> raw();

	/** Copy the buffer data into a new vector. */
	std::vector<uint8_t> toVector() const;

	// JS get internalArrayBuffer() returns this._buf.buffer (an ArrayBuffer object).
	// C++ returns _buf.data() (a raw uint8_t*) — a different type with different semantics.
	const uint8_t* internalArrayBuffer() const;

	// -----------------------------------------------------------------------
	// Position methods
	// -----------------------------------------------------------------------

	/**
	 * Set the absolute position of this buffer.
	 * Negative values will set the position from the end of the buffer.
	 */
	void seek(int64_t ofs);

	/**
	 * Shift the position relative to the current offset.
	 * Positive numbers seek forward, negative seek backwards.
	 */
	void move(int64_t ofs);

	// -----------------------------------------------------------------------
	// Variable-length integer reads
	// -----------------------------------------------------------------------

	int64_t readIntLE(size_t byteLength);
	std::vector<int64_t> readIntLE(size_t byteLength, size_t count);

	uint64_t readUIntLE(size_t byteLength);
	std::vector<uint64_t> readUIntLE(size_t byteLength, size_t count);

	int64_t readIntBE(size_t byteLength);
	std::vector<int64_t> readIntBE(size_t byteLength, size_t count);

	uint64_t readUIntBE(size_t byteLength);
	std::vector<uint64_t> readUIntBE(size_t byteLength, size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 8-bit
	// -----------------------------------------------------------------------

	int8_t readInt8();
	std::vector<int8_t> readInt8(size_t count);

	uint8_t readUInt8();
	std::vector<uint8_t> readUInt8(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 16-bit
	// -----------------------------------------------------------------------

	int16_t readInt16LE();
	std::vector<int16_t> readInt16LE(size_t count);

	uint16_t readUInt16LE();
	std::vector<uint16_t> readUInt16LE(size_t count);

	int16_t readInt16BE();
	std::vector<int16_t> readInt16BE(size_t count);

	uint16_t readUInt16BE();
	std::vector<uint16_t> readUInt16BE(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 24-bit (stored in 32-bit)
	// -----------------------------------------------------------------------

	int32_t readInt24LE();
	std::vector<int32_t> readInt24LE(size_t count);

	uint32_t readUInt24LE();
	std::vector<uint32_t> readUInt24LE(size_t count);

	int32_t readInt24BE();
	std::vector<int32_t> readInt24BE(size_t count);

	uint32_t readUInt24BE();
	std::vector<uint32_t> readUInt24BE(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 32-bit
	// -----------------------------------------------------------------------

	int32_t readInt32LE();
	std::vector<int32_t> readInt32LE(size_t count);

	uint32_t readUInt32LE();
	std::vector<uint32_t> readUInt32LE(size_t count);

	int32_t readInt32BE();
	std::vector<int32_t> readInt32BE(size_t count);

	uint32_t readUInt32BE();
	std::vector<uint32_t> readUInt32BE(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 40-bit (stored in 64-bit)
	// -----------------------------------------------------------------------

	int64_t readInt40LE();
	std::vector<int64_t> readInt40LE(size_t count);

	uint64_t readUInt40LE();
	std::vector<uint64_t> readUInt40LE(size_t count);

	int64_t readInt40BE();
	std::vector<int64_t> readInt40BE(size_t count);

	uint64_t readUInt40BE();
	std::vector<uint64_t> readUInt40BE(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 48-bit (stored in 64-bit)
	// -----------------------------------------------------------------------

	int64_t readInt48LE();
	std::vector<int64_t> readInt48LE(size_t count);

	uint64_t readUInt48LE();
	std::vector<uint64_t> readUInt48LE(size_t count);

	int64_t readInt48BE();
	std::vector<int64_t> readInt48BE(size_t count);

	uint64_t readUInt48BE();
	std::vector<uint64_t> readUInt48BE(size_t count);

	// -----------------------------------------------------------------------
	// Fixed-width integer reads — 64-bit
	// -----------------------------------------------------------------------

	int64_t readInt64LE();
	std::vector<int64_t> readInt64LE(size_t count);

	uint64_t readUInt64LE();
	std::vector<uint64_t> readUInt64LE(size_t count);

	int64_t readInt64BE();
	std::vector<int64_t> readInt64BE(size_t count);

	uint64_t readUInt64BE();
	std::vector<uint64_t> readUInt64BE(size_t count);

	// -----------------------------------------------------------------------
	// Float / double reads
	// -----------------------------------------------------------------------

	float readFloatLE();
	std::vector<float> readFloatLE(size_t count);

	float readFloatBE();
	std::vector<float> readFloatBE(size_t count);

	double readDoubleLE();
	std::vector<double> readDoubleLE(size_t count);

	double readDoubleBE();
	std::vector<double> readDoubleBE(size_t count);

	// -----------------------------------------------------------------------
	// String / buffer reads
	// -----------------------------------------------------------------------

	/** Read a portion of this buffer as a hex string. */
	std::string readHexString(size_t length);

	/**
	 * Read a buffer from this buffer (wrapped).
	 * Reads remaining bytes when called with no arguments.
	 */
	BufferWrapper readBuffer();
	BufferWrapper readBuffer(size_t length, bool inflate = false);
	// JS readBuffer(length, wrap, inflate) returns either a BufferWrapper or a raw Buffer via
	// dynamic typing. C++ uses std::variant; call sites must use std::get<> or std::visit.
	std::variant<BufferWrapper, std::vector<uint8_t>> readBuffer(size_t length, bool wrap, bool inflate);

	/** Read a buffer from this buffer (raw, unwrapped). */
	std::vector<uint8_t> readBufferRaw(size_t length, bool inflate = false);

	/**
	 * Read a string from the buffer.
	 * Reads remaining bytes when called with no arguments.
	 * @param encoding Encoding parameter for JS API compatibility. Only 'utf8' is
	 *        used in practice in the JS source. Non-utf8 encodings are treated as
	 *        raw byte copies (identical behavior for ascii/latin1).
	 */
	std::string readString();
	std::string readString(size_t length, std::string_view encoding = "utf8");

	/** Read a null-terminated string from the buffer. */
	std::string readNullTerminatedString(std::string_view encoding = "utf8");

	/**
	 * Returns true if the buffer starts with any of the given string(s).
	 */
	bool startsWith(std::string_view input, std::string_view encoding = "utf8");
	bool startsWith(const std::vector<std::string_view>& input, std::string_view encoding = "utf8");

	/**
	 * Read a string from the buffer and parse it as JSON.
	 * Reads remaining bytes when called with no arguments.
	 */
	nlohmann::json readJSON();
	nlohmann::json readJSON(size_t length, std::string_view encoding = "utf8");

	/**
	 * Read the entire buffer split by lines (\r\n or \n).
	 * Preserves current offset of the wrapper.
	 */
	std::vector<std::string> readLines(std::string_view encoding = "utf8");

	/**
	 * Fill a buffer with the given value.
	 * Fills remaining bytes when length is omitted.
	 */
	void fill(uint8_t value);
	void fill(uint8_t value, size_t length);

	// -----------------------------------------------------------------------
	// Write methods — integers
	// -----------------------------------------------------------------------

	void writeInt8(int8_t value);
	void writeUInt8(uint8_t value);

	void writeInt16LE(int16_t value);
	void writeUInt16LE(uint16_t value);
	void writeInt16BE(int16_t value);
	void writeUInt16BE(uint16_t value);

	void writeInt24LE(int32_t value);
	void writeUInt24LE(uint32_t value);
	void writeInt24BE(int32_t value);
	void writeUInt24BE(uint32_t value);

	void writeInt32LE(int32_t value);
	void writeUInt32LE(uint32_t value);
	void writeInt32BE(int32_t value);
	void writeUInt32BE(uint32_t value);

	void writeInt40LE(int64_t value);
	void writeUInt40LE(uint64_t value);
	void writeInt40BE(int64_t value);
	void writeUInt40BE(uint64_t value);

	void writeInt48LE(int64_t value);
	void writeUInt48LE(uint64_t value);
	void writeInt48BE(int64_t value);
	void writeUInt48BE(uint64_t value);

	void writeBigInt64LE(int64_t value);
	void writeBigUInt64LE(uint64_t value);
	void writeBigInt64BE(int64_t value);
	void writeBigUInt64BE(uint64_t value);

	void writeFloatLE(float value);
	void writeFloatBE(float value);

	// -----------------------------------------------------------------------
	// Write methods — buffer
	// -----------------------------------------------------------------------

	/**
	 * Write the contents of a buffer to this buffer.
	 * @param buf Source buffer (BufferWrapper).
	 * @param copyLength Number of bytes to copy (0 = all remaining).
	 */
	void writeBuffer(BufferWrapper& buf, size_t copyLength = 0);

	/**
	 * Write raw bytes to this buffer.
	 * @param buf Source span.
	 * @param copyLength Number of bytes to copy (0 = all).
	 */
	void writeBuffer(std::span<const uint8_t> buf, size_t copyLength = 0);

	// JS writeToFile() is async (fs.promises + await). C++ uses std::ofstream synchronously, blocking the calling thread.
	void writeToFile(const std::filesystem::path& file);

	// -----------------------------------------------------------------------
	// Search
	// -----------------------------------------------------------------------

	/**
	 * Get the index of the given char from start.
	 * Defaults to the current reader offset.
	 */
	int64_t indexOfChar(char ch);
	int64_t indexOfChar(char ch, size_t start);

	/**
	 * Get the index of the given byte from start.
	 * Defaults to the current reader offset.
	 */
	int64_t indexOf(uint8_t byte);
	int64_t indexOf(uint8_t byte, size_t start);

	// -----------------------------------------------------------------------
	// Utility
	// -----------------------------------------------------------------------

	/**
	 * Assign an object URL for this buffer.
	 * Creates one if it doesn't already exist.
	 */
	const std::string& getDataURL();

	/** Returns true if this buffer already has a cached/assigned object URL. */
	bool hasDataURL() const;

	/** Assign a precomputed object URL (JS-equivalent of setting this.dataURL). */
	void setDataURL(std::string url);

	/** Revoke the object URL assigned to this buffer. */
	void revokeDataURL();

	/** Returns the entire buffer encoded as base64. */
	std::string toBase64() const;

	// JS decodeAudio(context) takes an AudioContext and returns a Promise via the Web Audio API.
	// C++ takes no parameters and returns DecodedAudioData synchronously using miniaudio.
	DecodedAudioData decodeAudio() const;

	/**
	 * Replace the internal buffer with a different capacity.
	 * If the specified capacity is lower than the current, there may be data loss.
	 * @param capacity New capacity of the internal buffer.
	 * @param secure   If true, expanded capacity will be zeroed for security.
	 */
	void setCapacity(size_t capacity, bool secure = false);

	/**
	 * Calculate a hash of this buffer.
	 * @param hash     Hashing method, defaults to "md5".
	 * @param encoding Output encoding, defaults to "hex".
	 */
	std::string calculateHash(std::string_view hash = "md5", std::string_view encoding = "hex");

	/** Check if this buffer is entirely zeroed. */
	bool isZeroed() const;

	/** Get the CRC32 checksum for this buffer. */
	uint32_t getCRC32() const;

	/** Unmap underlying memory-mapped file if present. */
	void unmapSource();

	/**
	 * Returns a new deflated buffer using the contents of this buffer.
	 */
	BufferWrapper deflate() const;

protected:
	/**
	 * Check a given length does not exceed remaining capacity.
	 * Virtual so that BLTEReader can override to lazily decompress blocks
	 * on demand (matching the original JS behaviour).
	 * @param length Number of bytes required.
	 */
	virtual void _checkBounds(size_t length);

private:
	size_t _ofs = 0;
	uint8_t* _data = nullptr;
	size_t _size = 0;
	std::vector<uint8_t> _vec;
	std::unique_ptr<uint8_t[]> _raw;
	void* _mmap = nullptr;
	size_t _mmapSize = 0;
	std::optional<std::string> dataURL;
};

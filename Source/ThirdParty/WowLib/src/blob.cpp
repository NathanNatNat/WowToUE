/*!
 * Blob.js Polyfill for wow.export
 * Adapted from: https://github.com/eligrey/Blob.js by Kruithne <kruithne@gmail.com>
 * By Eli Grey, https://eligrey.com
 * By Jimmy Wärting, https://github.com/jimmywarting
 * License: MIT
 */
#include "blob.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <format>
#include <mutex>
#include <unordered_map>
#include <atomic>

static std::string array2base64(std::span<const uint8_t> input) {
	static constexpr char byteToCharMap[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

	std::string output;
	output.reserve(((input.size() + 2) / 3) * 4);

	for (std::size_t i = 0; i < input.size(); i += 3) {
		const uint8_t byte1 = input[i];
		const bool haveByte2 = i + 1 < input.size();
		const uint8_t byte2 = haveByte2 ? input[i + 1] : 0;
		const bool haveByte3 = i + 2 < input.size();
		const uint8_t byte3 = haveByte3 ? input[i + 2] : 0;

		const uint8_t outByte1 = byte1 >> 2;
		const uint8_t outByte2 = ((byte1 & 0x03) << 4) | (byte2 >> 4);
		uint8_t outByte3 = ((byte2 & 0x0F) << 2) | (byte3 >> 6);
		uint8_t outByte4 = byte3 & 0x3F;

		if (!haveByte3) {
			outByte4 = 64;
			if (!haveByte2)
				outByte3 = 64;
		}

		output += byteToCharMap[outByte1];
		output += byteToCharMap[outByte2];
		output += byteToCharMap[outByte3];
		output += byteToCharMap[outByte4];
	}

	return output;
}

static std::vector<uint8_t> stringEncode(std::string_view str) {
	return std::vector<uint8_t>(
		reinterpret_cast<const uint8_t*>(str.data()),
		reinterpret_cast<const uint8_t*>(str.data()) + str.size());
}

static std::string stringDecode(std::span<const uint8_t> buf) {
	const std::size_t end = buf.size();
	std::string result;
	result.reserve(end);

	std::size_t i = 0;
	while (i < end) {
		const uint8_t firstByte = buf[i];
		int bytesPerSequence = (firstByte > 0xEF) ? 4 :
			(firstByte > 0xDF) ? 3 :
			(firstByte > 0xBF) ? 2 : 1;

		uint32_t codePoint = 0;
		bool valid = false;

		if (i + bytesPerSequence <= end) {
			switch (bytesPerSequence) {
				case 1:
					if (firstByte < 0x80) {
						codePoint = firstByte;
						valid = true;
					}
					break;
				case 2: {
					uint8_t secondByte = buf[i + 1];
					if ((secondByte & 0xC0) == 0x80) {
						uint32_t tempCodePoint = (static_cast<uint32_t>(firstByte & 0x1F) << 6)
							| (secondByte & 0x3F);
						if (tempCodePoint > 0x7F) {
							codePoint = tempCodePoint;
							valid = true;
						}
					}
					break;
				}
				case 3: {
					uint8_t secondByte = buf[i + 1];
					uint8_t thirdByte = buf[i + 2];
					if ((secondByte & 0xC0) == 0x80 && (thirdByte & 0xC0) == 0x80) {
						uint32_t tempCodePoint = (static_cast<uint32_t>(firstByte & 0x0F) << 12)
							| (static_cast<uint32_t>(secondByte & 0x3F) << 6)
							| (thirdByte & 0x3F);
						if (tempCodePoint > 0x7FF && (tempCodePoint < 0xD800 || tempCodePoint > 0xDFFF)) {
							codePoint = tempCodePoint;
							valid = true;
						}
					}
					break;
				}
				case 4: {
					uint8_t secondByte = buf[i + 1];
					uint8_t thirdByte = buf[i + 2];
					uint8_t fourthByte = buf[i + 3];
					if ((secondByte & 0xC0) == 0x80 && (thirdByte & 0xC0) == 0x80 && (fourthByte & 0xC0) == 0x80) {
						uint32_t tempCodePoint = (static_cast<uint32_t>(firstByte & 0x0F) << 18)
							| (static_cast<uint32_t>(secondByte & 0x3F) << 12)
							| (static_cast<uint32_t>(thirdByte & 0x3F) << 6)
							| (fourthByte & 0x3F);
						if (tempCodePoint > 0xFFFF && tempCodePoint < 0x110000) {
							codePoint = tempCodePoint;
							valid = true;
						}
					}
					break;
				}
			}
		}

		if (!valid) {
			result += '\xEF';
			result += '\xBF';
			result += '\xBD';
			i += 1;
		} else {
			if (codePoint < 0x80) {
				result += static_cast<char>(codePoint);
			} else if (codePoint < 0x800) {
				result += static_cast<char>(0xC0 | (codePoint >> 6));
				result += static_cast<char>(0x80 | (codePoint & 0x3F));
			} else if (codePoint < 0x10000) {
				result += static_cast<char>(0xE0 | (codePoint >> 12));
				result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				result += static_cast<char>(0x80 | (codePoint & 0x3F));
			} else {
				result += static_cast<char>(0xF0 | (codePoint >> 18));
				result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
				result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				result += static_cast<char>(0x80 | (codePoint & 0x3F));
			}
			i += bytesPerSequence;
		}
	}

	return result;
}

static auto& textEncode = stringEncode;
static auto& textDecode = stringDecode;

struct ObjectUrlEntry {
	std::vector<uint8_t> bytes;
	std::string type;
};

std::mutex g_objectUrlMutex;
std::unordered_map<std::string, ObjectUrlEntry> g_objectUrls;
std::atomic<uint64_t> g_objectUrlCounter = 0;

static std::vector<uint8_t> bufferClone(std::span<const uint8_t> buf) {
	return std::vector<uint8_t>(buf.begin(), buf.end());
}

static std::vector<uint8_t> concatTypedarrays(const std::vector<std::vector<uint8_t>>& chunks) {
	std::size_t total_size = 0;
	for (const auto& chunk : chunks)
		total_size += chunk.size();

	std::vector<uint8_t> result;
	result.reserve(total_size);

	for (const auto& chunk : chunks)
		result.insert(result.end(), chunk.begin(), chunk.end());

	return result;
}

BlobPart::BlobPart(std::span<const uint8_t> data)
	: bytes(data.begin(), data.end()) {}

BlobPart::BlobPart(const std::vector<uint8_t>& data)
	: bytes(data) {}

BlobPart::BlobPart(std::vector<uint8_t>&& data)
	: bytes(std::move(data)) {}

BlobPart::BlobPart(std::string_view str)
	: bytes(textEncode(str)) {}

BlobPart::BlobPart(const BlobPolyfill& blob)
	: bytes(blob.arrayBuffer()) {}

BlobPolyfill::BlobPolyfill()
	: _buffer(), _type() {}

BlobPolyfill::BlobPolyfill(std::vector<BlobPart> parts, BlobOptions opts) {
	std::vector<std::vector<uint8_t>> chunks;
	chunks.reserve(parts.size());
	for (auto& part : parts)
		chunks.push_back(std::move(part.bytes));

	_buffer = concatTypedarrays(chunks);

	_type = opts.type;

	if (std::any_of(_type.begin(), _type.end(),
	                [](unsigned char c) { return c < 0x20 || c > 0x7E; })) {
		_type.clear();
	} else {
		std::transform(_type.begin(), _type.end(), _type.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	}
}

const std::vector<uint8_t>& BlobPolyfill::arrayBuffer() const {
	return _buffer;
}

std::string BlobPolyfill::text() const {
	return textDecode(_buffer);
}

BlobPolyfill BlobPolyfill::slice(std::size_t start,
                                 std::optional<std::size_t> end,
                                 const std::string& type) const {
	std::size_t actual_end = (end.has_value() && end.value() != 0) ? end.value() : _buffer.size();
	if (actual_end > _buffer.size())
		actual_end = _buffer.size();
	if (start > actual_end)
		start = actual_end;

	std::vector<uint8_t> slice_data(_buffer.begin() + static_cast<std::ptrdiff_t>(start),
	                                _buffer.begin() + static_cast<std::ptrdiff_t>(actual_end));
	return BlobPolyfill({BlobPart(std::move(slice_data))}, {type});
}

std::string BlobPolyfill::toString() const {
	return "[object Blob]";
}

BlobReadableStream BlobPolyfill::stream() const {
	return BlobReadableStream(this);
}

void BlobPolyfill::stream(std::function<void(std::span<const uint8_t>)> callback) const {
	BlobReadableStream readable = stream();
	while (auto chunk = readable.pull())
		callback(std::span<const uint8_t>(chunk->data(), chunk->size()));
}

std::size_t BlobPolyfill::size() const {
	return _buffer.size();
}

const std::string& BlobPolyfill::type() const {
	return _type;
}

BlobReadableStream::BlobReadableStream(const BlobPolyfill* blob)
	: _blob(blob), _position(0) {}

std::optional<std::vector<uint8_t>> BlobReadableStream::pull() {
	constexpr std::size_t CHUNK_SIZE = 524288;
	if (_blob == nullptr || _position >= _blob->size())
		return std::nullopt;

	const auto& source = _blob->arrayBuffer();
	const std::size_t chunk_end = std::min(_position + CHUNK_SIZE, source.size());
	std::vector<uint8_t> chunk(source.begin() + static_cast<std::ptrdiff_t>(_position),
	                           source.begin() + static_cast<std::ptrdiff_t>(chunk_end));
	_position = chunk_end;
	return chunk;
}

bool BlobReadableStream::closed() const {
	return _blob == nullptr || _position >= _blob->size();
}

std::string URLPolyfill::createObjectURL(const BlobPolyfill& blob) {
	return "data:" + blob.type() + ";base64," + array2base64(blob.arrayBuffer());
}

std::string URLPolyfill::createObjectURL(std::span<const uint8_t> bytes, std::string_view type) {
	const std::string url = std::format("blob:wow.export.cpp/{}", g_objectUrlCounter.fetch_add(1, std::memory_order_relaxed));
	ObjectUrlEntry entry;
	entry.bytes.assign(bytes.begin(), bytes.end());
	entry.type = std::string(type);

	std::lock_guard<std::mutex> lock(g_objectUrlMutex);
	g_objectUrls[url] = std::move(entry);
	return url;
}

std::optional<BlobPolyfill> URLPolyfill::resolveObjectURL(const std::string& url) {
	std::lock_guard<std::mutex> lock(g_objectUrlMutex);
	const auto it = g_objectUrls.find(url);
	if (it == g_objectUrls.end())
		return std::nullopt;
	return BlobPolyfill({BlobPart(it->second.bytes)}, BlobOptions{it->second.type});
}

void URLPolyfill::revokeObjectURL(const std::string& url) {
	if (!url.starts_with("data:")) {
		std::lock_guard<std::mutex> lock(g_objectUrlMutex);
		g_objectUrls.erase(url);
	}
}

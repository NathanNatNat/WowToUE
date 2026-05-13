/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "png-writer.h"

#include <cstdlib>
#include <cmath>
#include <limits>
#include <future>

namespace {


inline int paeth(int left, int up, int upLeft) {
	int p = left + up - upLeft;
	int paethLeft = std::abs(p - left);
	int paethUp = std::abs(p - up);
	int paethUpLeft = std::abs(p - upLeft);

	if (paethLeft <= paethUp && paethLeft <= paethUpLeft)
		return left;

	if (paethUp <= paethUpLeft)
		return up;

	return upLeft;
}


// None
void filter_none(const uint8_t* data, size_t dataOfs, size_t byteWidth,
                 uint8_t* raw, size_t rawOfs, size_t /*bytesPerPixel*/) {
	for (size_t x = 0; x < byteWidth; x++)
		raw[rawOfs + x] = data[dataOfs + x];
}

// Sub
void filter_sub(const uint8_t* data, size_t dataOfs, size_t byteWidth,
                uint8_t* raw, size_t rawOfs, size_t bytesPerPixel) {
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int value = data[dataOfs + x] - left;

		raw[rawOfs + x] = static_cast<uint8_t>(value);
	}
}

// Up
void filter_up(const uint8_t* data, size_t dataOfs, size_t byteWidth,
               uint8_t* raw, size_t rawOfs, size_t /*bytesPerPixel*/) {
	for (size_t x = 0; x < byteWidth; x++) {
		int up = dataOfs > 0 ? data[dataOfs + x - byteWidth] : 0;
		int value = data[dataOfs + x] - up;

		raw[rawOfs + x] = static_cast<uint8_t>(value);
	}
}

// Average
void filter_average(const uint8_t* data, size_t dataOfs, size_t byteWidth,
                    uint8_t* raw, size_t rawOfs, size_t bytesPerPixel) {
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int up = dataOfs > 0 ? data[dataOfs + x - byteWidth] : 0;
		int value = data[dataOfs + x] - ((left + up) >> 1);

		raw[rawOfs + x] = static_cast<uint8_t>(value);
	}
}

// Paeth
void filter_paeth(const uint8_t* data, size_t dataOfs, size_t byteWidth,
                  uint8_t* raw, size_t rawOfs, size_t bytesPerPixel) {
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int up = dataOfs > 0 ? data[dataOfs + x - byteWidth] : 0;
		int upLeft = dataOfs > 0 && x >= bytesPerPixel ? data[dataOfs + x - (byteWidth + bytesPerPixel)] : 0;
		int value = data[dataOfs + x] - paeth(left, up, upLeft);

		raw[rawOfs + x] = static_cast<uint8_t>(value);
	}
}

using FilterFunc = void(*)(const uint8_t*, size_t, size_t, uint8_t*, size_t, size_t);

constexpr FilterFunc FILTERS[5] = {
	filter_none,
	filter_sub,
	filter_up,
	filter_average,
	filter_paeth
};


// None
int64_t filter_sum_none(const uint8_t* data, size_t dataOfs, size_t byteWidth, size_t /*bytesPerPixel*/) {
	int64_t sum = 0;
	for (size_t i = dataOfs, len = dataOfs + byteWidth; i < len; i++)
		sum += std::abs(static_cast<int>(data[i]));

	return sum;
}

// Sub
int64_t filter_sum_sub(const uint8_t* data, size_t dataOfs, size_t byteWidth, size_t bytesPerPixel) {
	int64_t sum = 0;
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int value = data[dataOfs + x] - left;

		sum += std::abs(value);
	}

	return sum;
}

// Up
int64_t filter_sum_up(const uint8_t* data, size_t dataOfs, size_t byteWidth, size_t /*bytesPerPixel*/) {
	int64_t sum = 0;
	for (size_t x = dataOfs, len = dataOfs + byteWidth; x < len; x++) {
		int up = dataOfs > 0 ? data[x - byteWidth] : 0;
		int value = data[x] - up;

		sum += std::abs(value);
	}

	return sum;
}

// Average
int64_t filter_sum_average(const uint8_t* data, size_t dataOfs, size_t byteWidth, size_t bytesPerPixel) {
	int64_t sum = 0;
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int up = dataOfs > 0 ? data[dataOfs + x - byteWidth] : 0;
		int value = data[dataOfs + x] - ((left + up) >> 1);

		sum += std::abs(value);
	}

	return sum;
}

// Paeth
int64_t filter_sum_paeth(const uint8_t* data, size_t dataOfs, size_t byteWidth, size_t bytesPerPixel) {
	int64_t sum = 0;
	for (size_t x = 0; x < byteWidth; x++) {
		int left = x >= bytesPerPixel ? data[dataOfs + x - bytesPerPixel] : 0;
		int up = dataOfs > 0 ? data[dataOfs + x - byteWidth] : 0;
		int upLeft = dataOfs > 0 && x >= bytesPerPixel ? data[dataOfs + x - (byteWidth + bytesPerPixel)] : 0;
		int value = data[dataOfs + x] - paeth(left, up, upLeft);

		sum += std::abs(value);
	}

	return sum;
}

using FilterSumFunc = int64_t(*)(const uint8_t*, size_t, size_t, size_t);

constexpr FilterSumFunc FILTER_SUMS[5] = {
	filter_sum_none,
	filter_sum_sub,
	filter_sum_up,
	filter_sum_average,
	filter_sum_paeth
};

constexpr size_t NUM_FILTERS = 5;

/**
 * Apply adaptive filtering to image data.
 * @param {Buffer} data
 * @param {number} width
 * @param {number} height
 * @param {number} bytesPerPixel
 * @returns {Buffer}
 */
std::vector<uint8_t> filter(const uint8_t* data, uint32_t width, uint32_t height, uint32_t bytesPerPixel) {
	size_t byteWidth = static_cast<size_t>(width) * bytesPerPixel;
	size_t dataOfs = 0;

	size_t rawOfs = 0;
	std::vector<uint8_t> raw((byteWidth + 1) * height, 0);

	int selectedFilter = 0;
	for (uint32_t y = 0; y < height; y++) {
		int64_t min = std::numeric_limits<int64_t>::max();

		for (size_t i = 0; i < NUM_FILTERS; i++) {
			int64_t sum = FILTER_SUMS[i](data, dataOfs, byteWidth, bytesPerPixel);
			if (sum < min) {
				selectedFilter = static_cast<int>(i);
				min = sum;
			}
		}

		raw[rawOfs] = static_cast<uint8_t>(selectedFilter);
		rawOfs++;
	
		FILTERS[selectedFilter](data, dataOfs, byteWidth, raw.data(), rawOfs, bytesPerPixel);
		rawOfs += byteWidth;
		dataOfs += byteWidth;
	}
	return raw;
}

}

/**
 * Construct a new PNGWriter instance.
 * @param {number} width
 * @param {number} height
 */
PNGWriter::PNGWriter(uint32_t width, uint32_t height)
	: width(width),
	  height(height),
	  bytesPerPixel(4),
	  bitDepth(8),
	  colorType(6), // RGBA
	  data(static_cast<size_t>(width) * height * 4, 0) {}

/**
 * Get the internal pixel data for this PNG.
 */
std::vector<uint8_t>& PNGWriter::getPixelData() {
	return data;
}

/**
 * @returns {BufferWrapper}
 */
BufferWrapper PNGWriter::getBuffer() {
	BufferWrapper filtered(filter(data.data(), width, height, bytesPerPixel));
	BufferWrapper deflated = filtered.deflate();
	BufferWrapper buf = BufferWrapper::alloc(8 + 25 + deflated.byteLength() + 12 + 12, false);

	// 8-byte PNG signature.
	buf.writeUInt32LE(0x474E5089);
	buf.writeUInt32LE(0x0A1A0A0D);

	BufferWrapper ihdr = BufferWrapper::alloc(4 + 13, false);
	ihdr.writeUInt32LE(0x52444849); // IHDR
	ihdr.writeUInt32BE(width); // Image width
	ihdr.writeUInt32BE(height); // Image height
	ihdr.writeUInt8(bitDepth); // Bit-depth
	ihdr.writeUInt8(colorType); // Colour type
	ihdr.writeUInt8(0); // Compression (0)
	ihdr.writeUInt8(0); // Filter (0)
	ihdr.writeUInt8(0); // Interlace (0)
	ihdr.seek(0);

	buf.writeUInt32BE(13);
	buf.writeBuffer(ihdr);
	buf.writeInt32BE(static_cast<int32_t>(ihdr.getCRC32()));

	BufferWrapper idat = BufferWrapper::alloc(4 + deflated.byteLength(), false);
	idat.writeUInt32LE(0x54414449); // IDAT
	idat.writeBuffer(deflated);

	idat.seek(0);

	buf.writeUInt32BE(static_cast<uint32_t>(deflated.byteLength()));
	buf.writeBuffer(idat);
	buf.writeInt32BE(static_cast<int32_t>(idat.getCRC32()));

	buf.writeUInt32BE(0);
	buf.writeUInt32LE(0x444E4549); // IEND
	buf.writeUInt32LE(0x826042AE); // CRC IEND

	return buf;
}

/**
 * Write this PNG to a file.
 * @param {string} file
 */
std::shared_future<void> PNGWriter::write(const std::filesystem::path& file) {
	BufferWrapper buffer = getBuffer();
	return std::async(std::launch::async, [buffer = std::move(buffer), file]() mutable {
		buffer.writeToFile(file);
	}).share();
}

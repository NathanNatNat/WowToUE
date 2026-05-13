/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#include "blp.h"

#include "../buffer.h"
#include "../png-writer.h"

#ifndef WOWLIB_NO_WEBP
#include <webp/encode.h>
#endif
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <string>

namespace {

constexpr uint32_t DXT1 = 0x1;
constexpr uint32_t DXT3 = 0x2;
constexpr uint32_t DXT5 = 0x4;

constexpr uint32_t BLP_MAGIC = 0x32504c42;

/**
 * Unpack a colour value.
 * @param block  Raw data array.
 * @param index  Base index into block.
 * @param ofs    Additional offset.
 * @param colour Output colour array.
 * @param colourOfs Offset into colour array.
 * @returns The packed 16-bit colour value.
 */
uint16_t unpackColour(const std::vector<uint8_t>& block, int index, int ofs, int* colour, int colourOfs) {
	uint16_t value = static_cast<uint16_t>(block[index + ofs] | (block[index + 1 + ofs] << 8));

	int r = (value >> 11) & 0x1F;
	int g = (value >> 5) & 0x3F;
	int b = value & 0x1F;

	colour[colourOfs] = (r << 3) | (r >> 2);
	colour[colourOfs + 1] = (g << 2) | (g >> 4);
	colour[colourOfs + 2] = (b << 3) | (b >> 2);
	colour[colourOfs + 3] = 255;

	return value;
}

} // anonymous namespace

namespace casc {

BLPImage::BLPImage(BufferWrapper data)
	: data_(std::move(data)) {
	// Check magic value..
	if (data_.readUInt32LE() != BLP_MAGIC)
		throw std::runtime_error("Provided data is not a BLP file (invalid header magic).");

	// Check the BLP file type..
	uint32_t type = data_.readUInt32LE();
	if (type != 1)
		throw std::runtime_error("Unsupported BLP type: " + std::to_string(type));

	// Read file flags..
	encoding = data_.readUInt8();
	alphaDepth = data_.readUInt8();
	alphaEncoding = data_.readUInt8();
	containsMipmaps = data_.readUInt8();

	// Read file dimensions..
	width = data_.readUInt32LE();
	height = data_.readUInt32LE();

	// Read mipmap data..
	{
		auto offsets = data_.readUInt32LE(16);
		mapOffsets.assign(offsets.begin(), offsets.end());
	}
	{
		auto sizes = data_.readUInt32LE(16);
		mapSizes.assign(sizes.begin(), sizes.end());
	}

	// Calculate available mipmaps..
	mapCount = 0;
	for (uint32_t ofs : mapOffsets) {
		if (ofs != 0)
			mapCount++;
	}

	// Read colour palette..
	if (encoding == 1) {
		palette_.resize(256);
		for (int i = 0; i < 256; i++)
			palette_[i] = data_.readUInt8(4);
	}
}

std::string BLPImage::getDataURL(uint8_t mask, int mipmap) {
	BufferWrapper pngBuf = toPNG(mask, mipmap);
	return pngBuf.getDataURL();
}

BufferWrapper BLPImage::toPNG(uint8_t mask, int mipmap) {
	_prepare(mipmap);

	PNGWriter png(scaledWidth_, scaledHeight_);
	auto& pixelData = png.getPixelData();

	switch (encoding) {
		case 1: _getUncompressed(pixelData.data(), mask); break;
		case 2: _getCompressed(pixelData.data(), mask); break;
		case 3: _marshalBGRA(pixelData.data(), mask); break;
	}

	return png.getBuffer();
}

void BLPImage::saveToPNG(const std::filesystem::path& file, uint8_t mask, int mipmap) {
	toPNG(mask, mipmap).writeToFile(file);
}

#ifndef WOWLIB_NO_WEBP
BufferWrapper BLPImage::toWebP(uint8_t mask, int mipmap, int quality) {
	_prepare(mipmap);

	std::vector<uint8_t> pixelData(scaledWidth_ * scaledHeight_ * 4, 0);

	switch (encoding) {
		case 1: _getUncompressed(pixelData.data(), mask); break;
		case 2: _getCompressed(pixelData.data(), mask); break;
		case 3: _marshalBGRA(pixelData.data(), mask); break;
	}

	uint8_t* output = nullptr;
	size_t outputSize = 0;

	if (quality == 100) {
		outputSize = WebPEncodeLosslessRGBA(
			pixelData.data(),
			static_cast<int>(scaledWidth_),
			static_cast<int>(scaledHeight_),
			static_cast<int>(scaledWidth_ * 4),
			&output
		);
	} else {
		outputSize = WebPEncodeRGBA(
			pixelData.data(),
			static_cast<int>(scaledWidth_),
			static_cast<int>(scaledHeight_),
			static_cast<int>(scaledWidth_ * 4),
			static_cast<float>(quality),
			&output
		);
	}

	if (outputSize == 0 || output == nullptr)
		throw std::runtime_error("WebP encoding failed");

	BufferWrapper result = BufferWrapper::from(std::span<const uint8_t>(output, outputSize));
	WebPFree(output);

	return result;
}

void BLPImage::saveToWebP(const std::filesystem::path& file, uint8_t mask, int mipmap, int quality) {
	BufferWrapper webpBuffer = toWebP(mask, mipmap, quality);
	webpBuffer.writeToFile(file);
}
#else
BufferWrapper BLPImage::toWebP(uint8_t, int, int) { throw std::runtime_error("WebP not available"); }
void BLPImage::saveToWebP(const std::filesystem::path&, uint8_t, int, int) { throw std::runtime_error("WebP not available"); }
#endif

void BLPImage::_prepare(int mipmap) {
	// Constrict the requested mipmap to a valid range..
	mipmap = std::max(0, std::min(mipmap, mapCount - 1));

	// Calculate the scaled dimensions..
	scale_ = static_cast<int>(std::pow(2, mipmap));
	scaledWidth_ = std::max(1u, width / static_cast<uint32_t>(scale_));
	scaledHeight_ = std::max(1u, height / static_cast<uint32_t>(scale_));
	scaledLength_ = scaledWidth_ * scaledHeight_;

	// Extract the raw data we need..
	data_.seek(mapOffsets[mipmap]);
	rawData_ = data_.readUInt8(mapSizes[mipmap]);
}

BufferWrapper BLPImage::toBuffer(int mipmap, uint8_t mask) {
	_prepare(mipmap);

	switch (encoding) {
		case 1: return _getUncompressed(nullptr, mask);
		case 2: return _getCompressed(nullptr, mask);
		case 3: return _marshalBGRA(nullptr, mask);
		default: return BufferWrapper();
	}
}

BufferWrapper BLPImage::getRawMipmap(int mipmap) {
	_prepare(mipmap);
	return BufferWrapper::from(std::span<const uint8_t>(rawData_.data(), rawData_.size()));
}

std::vector<BLPImage::RawMipmapLevel> BLPImage::get_raw_mipmaps() {
	std::vector<RawMipmapLevel> mipmaps;
	for (int i = 0; i < mapCount; i++) {
		_prepare(i);
		mipmaps.push_back({
			std::vector<uint8_t>(rawData_.begin(), rawData_.end()),
			scaledWidth_,
			scaledHeight_
		});
	}
	return mipmaps;
}

std::vector<uint8_t> BLPImage::toUInt8Array(int mipmap, uint8_t mask) {
	_prepare(mipmap);

	std::vector<uint8_t> arr(scaledWidth_ * scaledHeight_ * 4, 0);
	switch (encoding) {
		case 1: _getUncompressed(arr.data(), mask); break;
		case 2: _getCompressed(arr.data(), mask); break;
		case 3: _marshalBGRA(arr.data(), mask); break;
	}

	return arr;
}

uint8_t BLPImage::_getAlpha(int index) const {
	uint8_t byte;
	switch (alphaDepth) {
		case 1:
			byte = rawData_[scaledLength_ + static_cast<size_t>(std::floor(index / 8.0))];
			return (byte & (0x01 << (index % 8))) == 0 ? 0x00 : 0xFF;

		case 4:
			byte = rawData_[scaledLength_ + (index / 2)];
			return static_cast<uint8_t>((index % 2 == 0) ? ((byte & 0x0F) << 4) : (byte & 0xF0));

		case 8:
			return rawData_[scaledLength_ + index];

		default:
			return 0xFF;
	}
}

BufferWrapper BLPImage::_getCompressed(uint8_t* canvasData, uint8_t mask) {
	const uint32_t flags = alphaDepth > 1 ? (alphaEncoding == 7 ? DXT5 : DXT3) : DXT1;

	std::vector<uint8_t> ownedData;
	uint8_t* data = canvasData;
	if (!canvasData) {
		ownedData.resize(scaledWidth_ * scaledHeight_ * 4, 0);
		data = ownedData.data();
	}

	int pos = 0;
	const int blockBytes = (flags & DXT1) != 0 ? 8 : 16;
	int target[4 * 16] = {};

	for (uint32_t y = 0, sh = scaledHeight_; y < sh; y += 4) {
		for (uint32_t x = 0, sw = scaledWidth_; x < sw; x += 4) {
			int blockPos = 0;

			if (static_cast<size_t>(pos) >= rawData_.size())
				continue;

			int colourIndex = pos;
			if ((flags & (DXT3 | DXT5)) != 0)
				colourIndex += 8;

			// Decompress colour..
			bool isDXT1 = (flags & DXT1) != 0;
			int colours[16] = {};
			uint16_t a = unpackColour(rawData_, colourIndex, 0, colours, 0);
			uint16_t b = unpackColour(rawData_, colourIndex, 2, colours, 4);

			for (int i = 0; i < 3; i++) {
				int c = colours[i];
				int d = colours[i + 4];

				if (isDXT1 && a <= b) {
					colours[i + 8] = (c + d) / 2;
					colours[i + 12] = 0;
				} else {
					colours[i + 8] = (2 * c + d) / 3;
					colours[i + 12] = (c + 2 * d) / 3;
				}
			}

			colours[8 + 3] = 255;
			colours[12 + 3] = (isDXT1 && a <= b) ? 0 : 255;

			int index[16] = {};
			for (int i = 0; i < 4; i++) {
				uint8_t packed = rawData_[colourIndex + 4 + i];
				index[i * 4] = packed & 0x3;
				index[1 + i * 4] = (packed >> 2) & 0x3;
				index[2 + i * 4] = (packed >> 4) & 0x3;
				index[3 + i * 4] = (packed >> 6) & 0x3;
			}

			for (int i = 0; i < 16; i++) {
				int ofs = index[i] * 4;
				target[4 * i] = colours[ofs];
				target[4 * i + 1] = colours[ofs + 1];
				target[4 * i + 2] = colours[ofs + 2];
				target[4 * i + 3] = colours[ofs + 3];
			}

			if ((flags & DXT3) != 0) {
				for (int i = 0; i < 8; i++) {
					uint8_t quant = rawData_[pos + i];

					int low = (quant & 0x0F);
					int high = (quant & 0xF0);

					target[8 * i + 3] = (low | (low << 4));
					target[8 * i + 7] = (high | (high >> 4));
				}
			} else if ((flags & DXT5) != 0) {
				int a0 = rawData_[pos];
				int a1 = rawData_[pos + 1];

				int alphaColours[8] = {};
				alphaColours[0] = a0;
				alphaColours[1] = a1;

				if (a0 <= a1) {
					for (int i = 1; i < 5; i++)
						alphaColours[i + 1] = ((5 - i) * a0 + i * a1) / 5;

					alphaColours[6] = 0;
					alphaColours[7] = 255;
				} else {
					for (int i = 1; i < 7; i++)
						alphaColours[i + 1] = ((7 - i) * a0 + i * a1) / 7;
				}

				int indices[16] = {};
				int alphaBlockPos = 2;
				int indicesPos = 0;

				for (int i = 0; i < 2; i++) {
					int value = 0;
					for (int j = 0; j < 3; j++) {
						uint8_t byte = rawData_[pos + alphaBlockPos++];
						value |= (byte << (8 * j));
					}

					for (int j = 0; j < 8; j++)
						indices[indicesPos++] = (value >> (3 * j)) & 0x07;
				}

				for (int i = 0; i < 16; i++)
					target[4 * i + 3] = alphaColours[indices[i]];
			}

			for (uint32_t pY = 0; pY < 4; pY++) {
				for (uint32_t pX = 0; pX < 4; pX++) {
					uint32_t sX = x + pX;
					uint32_t sY = y + pY;

					if (sX < sw && sY < sh) {
						size_t pixel = 4 * (sw * sY + sX);
						data[pixel + 0] = static_cast<uint8_t>((mask & 0b1) ? target[blockPos + 0] : 0);
						data[pixel + 1] = static_cast<uint8_t>((mask & 0b10) ? target[blockPos + 1] : 0);
						data[pixel + 2] = static_cast<uint8_t>((mask & 0b100) ? target[blockPos + 2] : 0);
						data[pixel + 3] = static_cast<uint8_t>((mask & 0b1000) ? target[blockPos + 3] : 255);
					}
					blockPos += 4;
				}
			}
			pos += blockBytes;
		}
	}

	if (!canvasData)
		return BufferWrapper::from(std::move(ownedData));
	return BufferWrapper();
}

BufferWrapper BLPImage::_getUncompressed(uint8_t* canvasData, uint8_t mask) {
	if (canvasData) {
		for (uint32_t i = 0, n = scaledLength_; i < n; i++) {
			const size_t ofs = i * 4;
			const auto& colour = palette_[rawData_[i]];

			canvasData[ofs] = static_cast<uint8_t>((mask & 0b1) ? colour[2] : 0);
			canvasData[ofs + 1] = static_cast<uint8_t>((mask & 0b10) ? colour[1] : 0);
			canvasData[ofs + 2] = static_cast<uint8_t>((mask & 0b100) ? colour[0] : 0);
			canvasData[ofs + 3] = static_cast<uint8_t>((mask & 0b1000) ? _getAlpha(static_cast<int>(i)) : 255);
		}
		return BufferWrapper();
	} else {
		BufferWrapper buf = BufferWrapper::alloc(scaledLength_ * 4);
		for (uint32_t i = 0, n = scaledLength_; i < n; i++) {
			const auto& colour = palette_[rawData_[i]];
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b1) ? colour[2] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b10) ? colour[1] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b100) ? colour[0] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b1000) ? _getAlpha(static_cast<int>(i)) : 255));
		}
		buf.seek(0);
		return buf;
	}
}

BufferWrapper BLPImage::_marshalBGRA(uint8_t* canvasData, uint8_t mask) {
	const auto& rawRef = rawData_;

	if (canvasData) {
		for (size_t i = 0, n = rawRef.size() / 4; i < n; i++) {
			size_t ofs = i * 4;
			canvasData[ofs] = static_cast<uint8_t>((mask & 0b1) ? rawRef[ofs + 2] : 0);
			canvasData[ofs + 1] = static_cast<uint8_t>((mask & 0b10) ? rawRef[ofs + 1] : 0);
			canvasData[ofs + 2] = static_cast<uint8_t>((mask & 0b100) ? rawRef[ofs] : 0);
			canvasData[ofs + 3] = static_cast<uint8_t>((mask & 0b1000) ? rawRef[ofs + 3] : 255);
		}
		return BufferWrapper();
	} else {
		BufferWrapper buf = BufferWrapper::alloc(rawRef.size());
		for (size_t i = 0, n = rawRef.size() / 4; i < n; i++) {
			size_t ofs = i * 4;
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b1) ? rawRef[ofs + 2] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b10) ? rawRef[ofs + 1] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b100) ? rawRef[ofs] : 0));
			buf.writeUInt8(static_cast<uint8_t>((mask & 0b1000) ? rawRef[ofs + 3] : 255));
		}
		buf.seek(0);
		return buf;
	}
}

} // namespace casc
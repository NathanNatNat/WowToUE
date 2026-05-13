/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>

#include "../buffer.h"

namespace casc {

/**
 * BLP image decoder.
 *
 * Supports BLP2 images with uncompressed (palette), DXT-compressed, and
 * uncompressed BGRA encodings. Provides conversion to raw RGBA, PNG, and WebP.
 *
 * JS equivalent: class BLPImage — module.exports = BLPImage
 */
class BLPImage {
public:
	/**
	 * Construct a new BLPImage instance.
	 * @param data BLP file data.
	 */
	explicit BLPImage(BufferWrapper data);

	/**
	 * Retrieve this BLP as a PNG image.
	 * @param mask  Channel mask (0b1111 = RGBA).
	 * @param mipmap Mipmap level (0 = full resolution).
	 * @returns BufferWrapper containing the PNG file.
	 */
	BufferWrapper toPNG(uint8_t mask = 0b1111, int mipmap = 0);

	/**
	 * Save this BLP as PNG file.
	 * @param file   Output path.
	 * @param mask   Channel mask.
	 * @param mipmap Mipmap level.
	 */
	void saveToPNG(const std::filesystem::path& file, uint8_t mask = 0b1111, int mipmap = 0);

	/**
	 * Convert this BLP to WebP format.
	 * @param mask    Channel mask.
	 * @param mipmap  Mipmap level.
	 * @param quality Quality setting (1-100), 100 = lossless.
	 * @returns BufferWrapper containing the WebP data.
	 */
	BufferWrapper toWebP(uint8_t mask = 0b1111, int mipmap = 0, int quality = 90);

	/**
	 * Save this BLP as WebP file.
	 * @param file    Output path.
	 * @param mask    Channel mask.
	 * @param mipmap  Mipmap level.
	 * @param quality Quality setting (1-100), 100 = lossless.
	 */
	void saveToWebP(const std::filesystem::path& file, uint8_t mask = 0b1111, int mipmap = 0, int quality = 90);

	/**
	 * Get the contents of this BLP as a BufferWrapper instance (raw RGBA).
	 * @param mipmap Mipmap level.
	 * @param mask   Channel mask.
	 * @returns BufferWrapper containing raw RGBA data.
	 */
	BufferWrapper toBuffer(int mipmap = 0, uint8_t mask = 0b1111);

	/**
	 * Get the contents of this raw BLP mipmap as a BufferWrapper instance.
	 * @param mipmap Mipmap level.
	 * @returns BufferWrapper containing the raw mipmap data.
	 */
	BufferWrapper getRawMipmap(int mipmap = 0);

	struct RawMipmapLevel {
		std::vector<uint8_t> data;
		uint32_t width;
		uint32_t height;
	};

	std::vector<RawMipmapLevel> get_raw_mipmaps();

	/**
	 * Get the contents of this BLP as an RGBA uint8 array.
	 * @param mipmap Mipmap level.
	 * @param mask   Channel mask.
	 * @returns Vector of RGBA pixel data.
	 */
	std::vector<uint8_t> toUInt8Array(int mipmap = 0, uint8_t mask = 0b1111);

	/**
	 * Encode this image as a data URL and return it.
	 * @param mask   Channel mask.
	 * @param mipmap Mipmap level.
	 * @returns Data URL string.
	 */
	std::string getDataURL(uint8_t mask = 0b1111, int mipmap = 0);

	// Public fields matching JS properties.
	uint8_t encoding = 0;
	uint8_t alphaDepth = 0;
	uint8_t alphaEncoding = 0;
	uint8_t containsMipmaps = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint32_t> mapOffsets;
	std::vector<uint32_t> mapSizes;
	int mapCount = 0;

	/**
	 * Get the scaled width of the image (after mipmap scaling).
	 * Matches JS `blp.scaledWidth`.
	 */
	uint32_t getScaledWidth() const { return scaledWidth_; }

	/**
	 * Get the scaled height of the image (after mipmap scaling).
	 * Matches JS `blp.scaledHeight`.
	 */
	uint32_t getScaledHeight() const { return scaledHeight_; }

private:
	/**
	 * Prepare BLP for processing.
	 * @param mipmap Mipmap level.
	 */
	void _prepare(int mipmap = 0);

	/**
	 * Calculate the alpha using this file's alpha depth.
	 * @param index Alpha index.
	 */
	uint8_t _getAlpha(int index) const;

	/**
	 * Extract compressed data (DXT1/DXT3/DXT5).
	 * @param canvasData Pointer to output pixel data (or nullptr to allocate).
	 * @param mask       Channel mask.
	 * @returns BufferWrapper if canvasData is nullptr, otherwise void writes to canvasData.
	 */
	BufferWrapper _getCompressed(uint8_t* canvasData, uint8_t mask = 0b1111);

	/**
	 * Match the uncompressed data with the palette.
	 * @param canvasData Pointer to output pixel data (or nullptr to allocate).
	 * @param mask       Channel mask.
	 * @returns BufferWrapper if canvasData is nullptr, otherwise void writes to canvasData.
	 */
	BufferWrapper _getUncompressed(uint8_t* canvasData, uint8_t mask);

	/**
	 * Marshal a BGRA array into an RGBA ordered buffer.
	 * @param canvasData Pointer to output pixel data (or nullptr to allocate).
	 * @param mask       Channel mask.
	 * @returns BufferWrapper if canvasData is nullptr, otherwise void writes to canvasData.
	 */
	BufferWrapper _marshalBGRA(uint8_t* canvasData, uint8_t mask);

	BufferWrapper data_;
	std::vector<std::vector<uint8_t>> palette_;

	// Prepared state (set by _prepare).
	int scale_ = 1;
	uint32_t scaledWidth_ = 0;
	uint32_t scaledHeight_ = 0;
	uint32_t scaledLength_ = 0;
	std::vector<uint8_t> rawData_;
};

} // namespace casc

/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "vp9-avi-demuxer.h"
#include "blte-stream-reader.h"
#include "../log.h"

#include <cstring>
#include <format>
#include <stdexcept>

namespace casc {

namespace {
	uint32_t readU32LE(const uint8_t* p) {
		uint32_t val;
		std::memcpy(&val, p, sizeof(val));
		return val;
	}

	std::string fourcc_string(const uint8_t* p) {
		return std::string(reinterpret_cast<const char*>(p), 4);
	}
} // anonymous namespace

VP9AVIDemuxer::VP9AVIDemuxer(BLTEStreamReader& stream_reader)
	: reader(stream_reader), config(std::nullopt), frame_rate(30.0) { // default, will parse from avih chunk
}

std::optional<VP9Config> VP9AVIDemuxer::parse_header() {
	BufferWrapper first_block = reader.getBlock(0);
	const auto data = first_block.raw();

	// find avih chunk for frame rate
	int64_t offset = find_chunk(data, "avih");
	if (offset != -1) {
		const uint32_t micro_per_frame = readU32LE(data.data() + offset + 8);
		frame_rate = 1000000.0 / micro_per_frame;
	}

	// find strf chunk for dimensions and codec
	offset = find_chunk(data, "strf");
	if (offset != -1) {
		const uint32_t width = readU32LE(data.data() + offset + 12);
		const uint32_t height = readU32LE(data.data() + offset + 16);

		// verify VP9 codec
		const std::string codec_fourcc = fourcc_string(data.data() + offset + 24);

		if (codec_fourcc != "VP90") {
			throw std::runtime_error(
				std::format("unsupported codec: {} (expected VP90)", codec_fourcc));
		}

		config = VP9Config{
			.codec = "vp09.00.10.08", // VP9 profile 0, level 1.0, 8-bit
			.codedWidth = width,
			.codedHeight = height,
			.hardwareAcceleration = "prefer-hardware"
		};
	}

	return config;
}

int64_t VP9AVIDemuxer::find_chunk(std::span<const uint8_t> data, const char (&fourcc)[5]) {
	if (data.size() < 4)
		return -1;
	for (size_t i = 0; i + 4 < data.size(); i++) {
		if (data[i] == static_cast<uint8_t>(fourcc[0]) &&
			data[i + 1] == static_cast<uint8_t>(fourcc[1]) &&
			data[i + 2] == static_cast<uint8_t>(fourcc[2]) &&
			data[i + 3] == static_cast<uint8_t>(fourcc[3]))
			return static_cast<int64_t>(i);
	}
	return -1;
}

void VP9AVIDemuxer::extract_frames(const std::function<void(const FrameInfo&)>& callback) {
	int64_t timestamp = 0;
	const int64_t frame_duration = static_cast<int64_t>(1000000.0 / frame_rate); // microseconds
	bool first_block = true;
	std::vector<uint8_t> leftover_buffer;
	size_t block_num = 0;

	reader.streamBlocks([&](BufferWrapper& block) {
		block_num++;

		// combine leftover from previous block with current block
		const auto block_raw = block.raw();
		const uint8_t* parse_data;
		size_t parse_size;
		std::vector<uint8_t> combined;

		if (!leftover_buffer.empty()) {
			combined.reserve(leftover_buffer.size() + block_raw.size());
			combined.insert(combined.end(), leftover_buffer.begin(), leftover_buffer.end());
			combined.insert(combined.end(), block_raw.begin(), block_raw.end());
			leftover_buffer.clear();
			parse_data = combined.data();
			parse_size = combined.size();
		} else {
			parse_data = block_raw.data();
			parse_size = block_raw.size();
		}

		ParseResult result = parse_movi_frames_with_remainder(parse_data, parse_size, first_block);
		first_block = false;

		logging::write(std::format("block {}: parsed {} frames, remainder: {} bytes",
			block_num, result.frames.size(), result.hasRemainder ? result.remainder.size() : 0));

		// save unparsed data for next block
		if (result.hasRemainder && !result.remainder.empty())
			leftover_buffer = std::move(result.remainder);

		for (const auto& frame_data : result.frames) {
			callback(FrameInfo{
				.type = "key",
				.timestamp = timestamp,
				.duration = frame_duration,
				.data = frame_data
			});
			timestamp += frame_duration;
		}
	});

	logging::write(std::format("total blocks processed: {}", block_num));
}

ParseResult VP9AVIDemuxer::parse_movi_frames_with_remainder(const uint8_t* data, size_t data_size, bool skip_header) {
	ParseResult result;
	size_t offset = 0;

	// skip to movi chunk on first block
	if (skip_header) {
		std::vector<uint8_t> data_vec(data, data + data_size);
		const int64_t movi_offset = find_chunk(data_vec, "movi");
		if (movi_offset != -1) {
			offset = static_cast<size_t>(movi_offset) + 4;
			logging::write(std::format("found movi chunk at offset {}, starting parse at {}", movi_offset, offset));
		} else {
			logging::write("movi chunk not found in first block");
			return result;
		}
	}

	size_t last_valid_offset = offset;
	size_t chunks_found = 0;

	while (offset + 8 <= data_size) {
		// read chunk fourcc and size
		const std::string chunk_id = fourcc_string(data + offset);
		const uint32_t chunk_size = readU32LE(data + offset + 4);

		// check if we have enough data for this chunk
		if (offset + 8 + chunk_size > data_size) {
			// chunk spans block boundary, save remainder
			logging::write(std::format("chunk {} size {} spans boundary at offset {}", chunk_id, chunk_size, offset));
			break;
		}

		// validate chunk size is reasonable
		if (chunk_size == 0 || chunk_size > 10 * 1024 * 1024) {
			// skip invalid chunk, try to find next valid chunk
			logging::write(std::format("invalid chunk at offset {}: id={}, size={}", offset, chunk_id, chunk_size));
			offset++;
			continue;
		}

		chunks_found++;

		// video chunks: '00dc' (compressed) or '00db' (uncompressed/keyframe)
		if (chunk_id == "00dc" || chunk_id == "00db") {
			std::vector<uint8_t> frame(data + offset + 8, data + offset + 8 + chunk_size);
			if (!frame.empty())
				result.frames.push_back(std::move(frame));
		}

		offset += 8 + chunk_size;
		// AVI uses word alignment
		if (chunk_size % 2)
			offset++;

		last_valid_offset = offset;
	}

	logging::write(std::format("parsed {} chunks, {} video frames, consumed {} bytes of {}",
		chunks_found, result.frames.size(), last_valid_offset, data_size));

	// return unparsed data as remainder
	if (last_valid_offset < data_size) {
		result.remainder.assign(data + last_valid_offset, data + data_size);
		result.hasRemainder = true;
	}

	return result;
}

std::vector<std::vector<uint8_t>> VP9AVIDemuxer::parse_movi_frames(const uint8_t* data, size_t data_size, bool skip_header) {
	std::vector<std::vector<uint8_t>> frames;
	size_t offset = 0;

	// skip to movi chunk on first block
	if (skip_header) {
		std::vector<uint8_t> data_vec(data, data + data_size);
		const int64_t movi_offset = find_chunk(data_vec, "movi");
		if (movi_offset != -1)
			offset = static_cast<size_t>(movi_offset) + 4;
	}

	while (offset + 8 <= data_size) {
		// read chunk fourcc and size
		const std::string chunk_id = fourcc_string(data + offset);
		const uint32_t chunk_size = readU32LE(data + offset + 4);

		// check if we have enough data for this chunk
		if (offset + 8 + chunk_size > data_size) {
			// chunk spans block boundary, will be in next block
			break;
		}

		// validate chunk size is reasonable
		if (chunk_size == 0 || chunk_size > 10 * 1024 * 1024) {
			// skip invalid chunk, try to find next valid chunk
			offset++;
			continue;
		}

		// video chunks: '00dc' (compressed) or '00db' (uncompressed/keyframe)
		if (chunk_id == "00dc" || chunk_id == "00db") {
			std::vector<uint8_t> frame(data + offset + 8, data + offset + 8 + chunk_size);
			// only add non-empty frames
			if (!frame.empty())
				frames.push_back(std::move(frame));
		}

		offset += 8 + chunk_size;
		// AVI uses word alignment
		if (chunk_size % 2)
			offset++;
	}

	return frames;
}

} // namespace casc

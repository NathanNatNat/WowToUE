/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <functional>
#include <optional>

namespace casc {

class BLTEStreamReader;

struct VP9Config {
	std::string codec;
	uint32_t codedWidth = 0;
	uint32_t codedHeight = 0;
	std::string hardwareAcceleration;
};

struct FrameInfo {
	std::string type;
	int64_t timestamp = 0;
	int64_t duration = 0;
	std::vector<uint8_t> data;
};

struct ParseResult {
	std::vector<std::vector<uint8_t>> frames;
	std::vector<uint8_t> remainder;
	bool hasRemainder = false;
};

/**
 * VP9AVIDemuxer - Minimal AVI container parser for VP9-encoded video streams
 * Extracts video frames from VP9 AVI files for WebCodecs playback
 */
class VP9AVIDemuxer {
public:
	explicit VP9AVIDemuxer(BLTEStreamReader& stream_reader);

	std::optional<VP9Config> parse_header();

	int64_t find_chunk(std::span<const uint8_t> data, const char (&fourcc)[5]);

	void extract_frames(const std::function<void(const FrameInfo&)>& callback);

	ParseResult parse_movi_frames_with_remainder(const uint8_t* data, size_t data_size, bool skip_header);

	std::vector<std::vector<uint8_t>> parse_movi_frames(const uint8_t* data, size_t data_size, bool skip_header);

private:
	BLTEStreamReader& reader;
	std::optional<VP9Config> config;
	double frame_rate;
};

} // namespace casc

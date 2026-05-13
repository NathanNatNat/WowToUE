/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <variant>
#include <vector>

class BufferWrapper;

/**
 * Data types for M2 track reading.
 */
enum class M2DataType {
	uint8,
	uint32,
	int16,
	float3,
	float4,
	compquat
};

/**
 * A single value read from a track. The active variant depends on the M2DataType.
 * - uint8:    uint8_t
 * - uint32:   uint32_t
 * - int16:    int16_t
 * - float3:   std::vector<float>  (3 elements)
 * - float4:   std::vector<float>  (4 elements)
 * - compquat: std::vector<float>  (4 elements, normalized)
 */
using M2Value = std::variant<uint8_t, uint32_t, int16_t, std::vector<float>>;

/**
 * Per-animation offset info stored when storeOffsets is true.
 */
struct M2ArrayOffset {
	uint32_t count;
	uint32_t offset;
};

/**
 * Result type for read_m2_array_array when storeOffsets is true.
 */
struct M2ArrayArrayResult {
	std::vector<std::vector<M2Value>> arr;
	std::vector<M2ArrayOffset> offsets;
};

/**
 * Sequence entry used for external .anim file detection.
 */
struct M2Sequence {
	uint32_t flags;
};

class M2Track {
public:
	/**
	 * Default constructor — empty track.
	 */
	M2Track() : globalSeq(0), interpolation(0) {}

	/**
	 * Construct a new M2Track instance.
	 * @param globalSeq
	 * @param interpolation
	 * @param timestamps
	 * @param values
	 * @param timestampOffsets - array of {count, offset} per animation
	 * @param valueOffsets - array of {count, offset} per animation
	 */
	M2Track(uint16_t globalSeq, uint16_t interpolation,
	        std::vector<std::vector<M2Value>> timestamps,
	        std::vector<std::vector<M2Value>> values,
	        std::vector<M2ArrayOffset> timestampOffsets = {},
	        std::vector<M2ArrayOffset> valueOffsets = {});

	uint16_t globalSeq;
	uint16_t interpolation;
	std::vector<std::vector<M2Value>> timestamps;
	std::vector<std::vector<M2Value>> values;
	std::vector<M2ArrayOffset> timestampOffsets;
	std::vector<M2ArrayOffset> valueOffsets;
};

/**
 * Bounding box with min/max float3 vectors.
 * See https://wowdev.wiki/Common_Types#CAaBox
 */
struct CAaBB {
	std::vector<float> min;
	std::vector<float> max;
};

// See https://wowdev.wiki/M2#Standard_animation_block
std::vector<std::vector<M2Value>> read_m2_array_array(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims = false,
	const std::map<uint32_t, BufferWrapper*>& animFiles = {},
	const std::vector<M2Sequence>* sequences = nullptr);

M2ArrayArrayResult read_m2_array_array_with_offsets(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims = false,
	const std::map<uint32_t, BufferWrapper*>& animFiles = {},
	const std::vector<M2Sequence>* sequences = nullptr);

// See https://wowdev.wiki/M2#Standard_animation_block
M2Track read_m2_track(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims = false,
	const std::map<uint32_t, BufferWrapper*>& animFiles = {},
	bool storeOffsets = false,
	const std::vector<M2Sequence>* sequences = nullptr);

/**
 * Patch a single animation slot in a track using external .anim data.
 * @param track
 * @param animIndex
 * @param animBuffer
 * @param valueType
 */
void patch_track_animation(M2Track& track, uint32_t animIndex,
                           BufferWrapper& animBuffer, M2DataType valueType);

// See https://wowdev.wiki/Common_Types#CAaBox
CAaBB read_caa_bb(BufferWrapper& data);

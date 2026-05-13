/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */

#include "M2Generics.h"
#include "../../buffer.h"

#include <stdexcept>
#include <string>

M2Track::M2Track(uint16_t globalSeq, uint16_t interpolation,
                 std::vector<std::vector<M2Value>> timestamps,
                 std::vector<std::vector<M2Value>> values,
                 std::vector<M2ArrayOffset> timestampOffsets,
                 std::vector<M2ArrayOffset> valueOffsets)
	: globalSeq(globalSeq),
	  interpolation(interpolation),
	  timestamps(std::move(timestamps)),
	  values(std::move(values)),
	  timestampOffsets(std::move(timestampOffsets)),
	  valueOffsets(std::move(valueOffsets)) {
}

static M2Value read_value(BufferWrapper& buf, M2DataType dataType, const char* errorPrefix = "Unknown") {
	switch (dataType) {
		case M2DataType::uint32:
			return buf.readUInt32LE();
		case M2DataType::int16:
			return buf.readInt16LE();
		case M2DataType::float3:
			return buf.readFloatLE(3);
		case M2DataType::float4:
			return buf.readFloatLE(4);
		case M2DataType::compquat: {
			auto raw = buf.readUInt16LE(4);
			std::vector<float> result(4);
			for (int k = 0; k < 4; k++)
				result[k] = (static_cast<float>(raw[k]) - 32767.0f) / 32768.0f;
			return result;
		}
		case M2DataType::uint8:
			return buf.readUInt8();
		default:
			throw std::runtime_error(std::string(errorPrefix) + " data type: " + std::to_string(static_cast<int>(dataType)));
	}
}

static uint32_t value_size(M2DataType dataType) {
	switch (dataType) {
		case M2DataType::uint32:  return 4;
		case M2DataType::int16:   return 2;
		case M2DataType::float3:  return 12;
		case M2DataType::float4:  return 16;
		case M2DataType::compquat: return 8;
		case M2DataType::uint8:   return 1;
		default: return 0;
	}
}

// See https://wowdev.wiki/M2#Standard_animation_block
static std::vector<std::vector<M2Value>> read_m2_array_array_internal(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims,
	const std::map<uint32_t, BufferWrapper*>& animFiles,
	bool storeOffsets,
	const std::vector<M2Sequence>* sequences,
	std::vector<M2ArrayOffset>* outOffsets)
{
	const uint32_t arrCount = data.readUInt32LE();
	const uint32_t arrOfs = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(ofs + arrOfs);

	std::vector<std::vector<M2Value>> arr(arrCount);
	if (storeOffsets && outOffsets)
		outOffsets->resize(arrCount);

	for (uint32_t i = 0; i < arrCount; i++) {
		const uint32_t subArrCount = data.readUInt32LE();
		const uint32_t subArrOfs = data.readUInt32LE();
		const size_t subBase = data.offset();

		if (storeOffsets && outOffsets)
			(*outOffsets)[i] = { subArrCount, subArrOfs };

		// data lives in external .anim file, skip reading from M2 buffer
		if (sequences && i < sequences->size() && ((*sequences)[i].flags & 0x20) == 0) {
			arr[i] = {};
			data.seek(subBase);
			continue;
		}

		data.seek(ofs + subArrOfs);

		arr[i].resize(subArrCount);
		for (uint32_t j = 0; j < subArrCount; j++) {
			if (useAnims && animFiles.count(i)) {
				auto* animBuf = animFiles.at(i);
				const uint32_t elemSize = value_size(dataType);
				if (elemSize == 0) {
					arr[i][j] = read_value(*animBuf, dataType, "Unhandled");
				} else {
					animBuf->seek(subArrOfs + (j * elemSize));
					arr[i][j] = read_value(*animBuf, dataType, "Unhandled");
				}
			} else {
				arr[i][j] = read_value(data, dataType);
			}
		}

		data.seek(subBase);
	}

	data.seek(base);
	return arr;
}

// See https://wowdev.wiki/M2#Standard_animation_block
std::vector<std::vector<M2Value>> read_m2_array_array(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims,
	const std::map<uint32_t, BufferWrapper*>& animFiles,
	const std::vector<M2Sequence>* sequences)
{
	return read_m2_array_array_internal(data, ofs, dataType, useAnims, animFiles, false, sequences, nullptr);
}

M2ArrayArrayResult read_m2_array_array_with_offsets(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims,
	const std::map<uint32_t, BufferWrapper*>& animFiles,
	const std::vector<M2Sequence>* sequences)
{
	M2ArrayArrayResult result;
	result.arr = read_m2_array_array_internal(data, ofs, dataType, useAnims, animFiles, true, sequences, &result.offsets);
	return result;
}

// See https://wowdev.wiki/M2#Standard_animation_block
M2Track read_m2_track(
	BufferWrapper& data, uint32_t ofs, M2DataType dataType,
	bool useAnims,
	const std::map<uint32_t, BufferWrapper*>& animFiles,
	bool storeOffsets,
	const std::vector<M2Sequence>* sequences)
{
	static const std::map<uint32_t, BufferWrapper*> empty_anim_files{};

	const uint16_t interpolation = data.readUInt16LE();
	const uint16_t globalSeq = data.readUInt16LE();

	std::vector<std::vector<M2Value>> timestamps;
	std::vector<std::vector<M2Value>> values;
	std::vector<M2ArrayOffset> timestampOffsets;
	std::vector<M2ArrayOffset> valueOffsets;

	if (useAnims) {
		timestamps = read_m2_array_array(data, ofs, M2DataType::uint32, useAnims, animFiles, sequences);
		values = read_m2_array_array(data, ofs, dataType, useAnims, animFiles, sequences);
	} else if (storeOffsets) {
		auto tsResult = read_m2_array_array_with_offsets(data, ofs, M2DataType::uint32, false, animFiles, sequences);
		timestamps = std::move(tsResult.arr);
		timestampOffsets = std::move(tsResult.offsets);

		auto valResult = read_m2_array_array_with_offsets(data, ofs, dataType, false, animFiles, sequences);
		values = std::move(valResult.arr);
		valueOffsets = std::move(valResult.offsets);
	} else {
		timestamps = read_m2_array_array(data, ofs, M2DataType::uint32, false, empty_anim_files, sequences);
		values = read_m2_array_array(data, ofs, dataType, false, empty_anim_files, sequences);
	}

	return M2Track(globalSeq, interpolation,
	               std::move(timestamps), std::move(values),
	               std::move(timestampOffsets), std::move(valueOffsets));
}

/**
 * Patch a single animation slot in a track using external .anim data.
 * @param track
 * @param animIndex
 * @param animBuffer
 * @param valueType
 */
void patch_track_animation(M2Track& track, uint32_t animIndex,
                           BufferWrapper& animBuffer, M2DataType valueType)
{
	if (track.timestampOffsets.empty() || track.valueOffsets.empty())
		return;

	if (animIndex >= track.timestampOffsets.size())
		return;

	const auto& tsInfo = track.timestampOffsets[animIndex];
	const auto& valInfo = track.valueOffsets[animIndex];

	// bounds check: ensure offsets fit within the .anim buffer
	const uint32_t valElemSize = value_size(valueType);
	const uint32_t tsEnd = tsInfo.offset + tsInfo.count * 4;
	const uint32_t valEnd = valInfo.offset + valInfo.count * valElemSize;

	if (tsEnd > animBuffer.byteLength() || valEnd > animBuffer.byteLength())
		return;

	// read timestamps from .anim buffer
	std::vector<M2Value> timestamps(tsInfo.count);
	for (uint32_t j = 0; j < tsInfo.count; j++) {
		animBuffer.seek(tsInfo.offset + (j * 4));
		timestamps[j] = animBuffer.readUInt32LE();
	}
	track.timestamps[animIndex] = std::move(timestamps);

	// read values from .anim buffer
	std::vector<M2Value> values(valInfo.count);
	for (uint32_t j = 0; j < valInfo.count; j++) {
		const uint32_t elemSize = value_size(valueType);
		animBuffer.seek(valInfo.offset + (j * elemSize));
		values[j] = read_value(animBuffer, valueType);
	}

	track.values[animIndex] = std::move(values);
}

// See https://wowdev.wiki/Common_Types#CAaBox
CAaBB read_caa_bb(BufferWrapper& data) {
	return { data.readFloatLE(3), data.readFloatLE(3) };
}
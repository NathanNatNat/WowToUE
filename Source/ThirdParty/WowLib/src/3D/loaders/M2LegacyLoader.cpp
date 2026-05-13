#include "M2LegacyLoader.h"
#include "../Texture.h"
#include "../../buffer.h"

#include <algorithm>
#include <format>
#include <future>
#include <stdexcept>

M2LegacyLoader::M2LegacyLoader(BufferWrapper& data)
	: data(data) {
}

std::future<void> M2LegacyLoader::load() {
	return std::async(std::launch::deferred, [this]() {
		if (this->isLoaded)
			return;

		auto& data = this->data;

		const uint32_t magic = data.readUInt32LE();
		if (magic != MAGIC_MD20)
			throw std::runtime_error("Invalid M2 magic: 0x" + std::format("{:x}", magic));

		this->version = data.readUInt32LE();

		if (this->version < M2_VER_VANILLA_MIN || this->version > M2_VER_WOTLK)
			throw std::runtime_error("Unsupported M2 version: " + std::to_string(this->version));

		this->_parse_header();
		this->isLoaded = true;
	});
}

void M2LegacyLoader::_parse_header() {
	auto& data = this->data;
	const uint32_t ofs = 0; // legacy m2 has no chunk wrapper, offsets are from file start

	this->_parse_model_name(ofs);
	this->flags = data.readUInt32LE();
	this->_parse_global_loops(ofs);
	this->_parse_animations(ofs);
	this->_parse_animation_lookup(ofs);

	// playable animation lookup (vanilla only)
	if (this->version <= M2_VER_VANILLA_MAX)
		this->_parse_playable_animation_lookup(ofs);

	this->_parse_bones(ofs);

	// key bone lookup
	data.move(8);

	this->_parse_vertices(ofs);

	// views (skins) - inline for pre-wotlk
	if (this->version < M2_VER_WOTLK)
		this->_parse_views_inline(ofs);
	else
		this->viewCount = data.readUInt32LE();

	this->_parse_colors(ofs);
	this->_parse_textures(ofs);
	this->_parse_texture_weights(ofs);

	// texture flipbooks (vanilla only)
	if (this->version <= M2_VER_VANILLA_MAX)
		data.move(8);

	this->_parse_texture_transforms(ofs);
	this->_parse_replaceable_texture_lookup(ofs);
	this->_parse_materials(ofs);

	// bone combos
	data.move(8);

	this->_parse_texture_combos(ofs);

	// texture transform bone map
	data.move(8);

	this->_parse_transparency_lookup(ofs);
	this->_parse_texture_transform_lookup(ofs);
	this->_parse_bounding_box();
	this->_parse_collision(ofs);
	this->_parse_attachments(ofs);
}

void M2LegacyLoader::_parse_model_name(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t nameLength = data.readUInt32LE();
	const uint32_t nameOfs = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(nameOfs + ofs);
	this->name = data.readString(nameLength > 0 ? nameLength - 1 : 0);
	data.seek(base);
}

void M2LegacyLoader::_parse_global_loops(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->globalLoops = data.readUInt32LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_animations(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->animations.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		auto& anim = this->animations[i];

		if (this->version < M2_VER_WOTLK) {
			// pre-wotlk: single timeline with start/end timestamps
			anim.id = data.readUInt16LE();
			anim.variationIndex = data.readUInt16LE();
			anim.startTimestamp = data.readUInt32LE();
			anim.endTimestamp = data.readUInt32LE();
			anim.movespeed = data.readFloatLE();
			anim.flags = data.readUInt32LE();
			anim.frequency = data.readInt16LE();
			anim.padding = data.readUInt16LE();
			anim.replayMin = data.readUInt32LE();
			anim.replayMax = data.readUInt32LE();
			anim.blendTime = data.readUInt32LE();
			anim.boxMin = data.readFloatLE(3);
			anim.boxMax = data.readFloatLE(3);
			anim.boxRadius = data.readFloatLE();
			anim.variationNext = data.readInt16LE();
			anim.aliasNext = data.readUInt16LE();

			// compute duration from timestamps
			anim.duration = anim.endTimestamp - anim.startTimestamp;
		} else {
			// wotlk: per-animation timeline with duration
			anim.id = data.readUInt16LE();
			anim.variationIndex = data.readUInt16LE();
			anim.duration = data.readUInt32LE();
			anim.movespeed = data.readFloatLE();
			anim.flags = data.readUInt32LE();
			anim.frequency = data.readInt16LE();
			anim.padding = data.readUInt16LE();
			anim.replayMin = data.readUInt32LE();
			anim.replayMax = data.readUInt32LE();
			anim.blendTimeIn = data.readUInt16LE();
			anim.blendTimeOut = data.readUInt16LE();
			anim.boxMin = data.readFloatLE(3);
			anim.boxMax = data.readFloatLE(3);
			anim.boxRadius = data.readFloatLE();
			anim.variationNext = data.readInt16LE();
			anim.aliasNext = data.readUInt16LE();
		}
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_animation_lookup(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->animationLookup = data.readInt16LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_playable_animation_lookup(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	// playable animation lookup: fallback animation id + flags
	this->playableAnimationLookup.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		this->playableAnimationLookup[i] = {
			data.readUInt16LE(),
			data.readUInt16LE()
		};
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_bones(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->bones.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		auto& bone = this->bones[i];

		bone.boneID = data.readInt32LE();
		bone.flags = data.readUInt32LE();
		bone.parentBone = data.readInt16LE();
		bone.subMeshID = data.readUInt16LE();

		// bone name crc added in tbc
		if (this->version >= M2_VER_TBC_MIN)
			bone.boneNameCRC = data.readUInt32LE();

		bone.translation = this->_read_m2_track(data, ofs, LegacyDataType::float3);
		bone.rotation = this->_read_m2_track(data, ofs, LegacyDataType::compquat);
		bone.scale = this->_read_m2_track(data, ofs, LegacyDataType::float3);
		bone.pivot = data.readFloatLE(3);

		// convert coordinate system (wow z-up to webgl y-up)
		this->_convert_bone_coords(bone);
	}

	data.seek(base);
}

void M2LegacyLoader::_convert_bone_coords(LegacyM2Bone& bone) {
	auto& pivot = bone.pivot;

	// single-timeline: values is flat array, not per-animation
	if (this->version < M2_VER_WOTLK) {
		for (auto& val : bone.translation.flatValues) {
			auto& v = std::get<std::vector<float>>(val);
			const float dx = v[0];
			const float dy = v[1];
			const float dz = v[2];
			v[0] = dx;
			v[2] = dy * -1;
			v[1] = dz;
		}

		for (auto& val : bone.rotation.flatValues) {
			auto& v = std::get<std::vector<float>>(val);
			const float dx = v[0];
			const float dy = v[1];
			const float dz = v[2];
			const float dw = v[3];
			v[0] = dx;
			v[2] = dy * -1;
			v[1] = dz;
			v[3] = dw;
		}

		for (auto& val : bone.scale.flatValues) {
			auto& v = std::get<std::vector<float>>(val);
			const float dx = v[0];
			const float dy = v[1];
			const float dz = v[2];
			v[0] = dx;
			v[2] = dy;
			v[1] = dz;
		}
	} else {
		// per-animation timeline
		for (auto& animVals : bone.translation.nestedValues) {
			for (auto& val : animVals) {
				auto& v = std::get<std::vector<float>>(val);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy * -1;
				v[1] = dz;
			}
		}

		for (auto& animVals : bone.rotation.nestedValues) {
			for (auto& val : animVals) {
				auto& v = std::get<std::vector<float>>(val);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				const float dw = v[3];
				v[0] = dx;
				v[2] = dy * -1;
				v[1] = dz;
				v[3] = dw;
			}
		}

		for (auto& animVals : bone.scale.nestedValues) {
			for (auto& val : animVals) {
				auto& v = std::get<std::vector<float>>(val);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy;
				v[1] = dz;
			}
		}
	}

	// pivot
	const float pivotX = pivot[0];
	const float pivotY = pivot[1];
	const float pivotZ = pivot[2];
	pivot[0] = pivotX;
	pivot[2] = pivotY * -1;
	pivot[1] = pivotZ;
}

void M2LegacyLoader::_parse_vertices(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->vertices.resize(count * 3);
	this->normals.resize(count * 3);
	this->uv.resize(count * 2);
	this->uv2.resize(count * 2);
	this->boneWeights.resize(count * 4);
	this->boneIndices.resize(count * 4);

	for (uint32_t i = 0; i < count; i++) {
		// position (convert z-up to y-up)
		this->vertices[i * 3] = data.readFloatLE();
		this->vertices[i * 3 + 2] = data.readFloatLE() * -1;
		this->vertices[i * 3 + 1] = data.readFloatLE();

		// bone weights
		for (int x = 0; x < 4; x++)
			this->boneWeights[i * 4 + x] = data.readUInt8();

		// bone indices
		for (int x = 0; x < 4; x++)
			this->boneIndices[i * 4 + x] = data.readUInt8();

		// normals (convert z-up to y-up)
		this->normals[i * 3] = data.readFloatLE();
		this->normals[i * 3 + 2] = data.readFloatLE() * -1;
		this->normals[i * 3 + 1] = data.readFloatLE();

		this->uv[i * 2] = data.readFloatLE();
		this->uv[i * 2 + 1] = data.readFloatLE();

		this->uv2[i * 2] = data.readFloatLE();
		this->uv2[i * 2 + 1] = data.readFloatLE();
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_views_inline(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	this->viewCount = count;
	this->skins.resize(count);

	const size_t base = data.offset();
	data.seek(offset + ofs);

	for (uint32_t v = 0; v < count; v++) {
		auto& skin = this->skins[v];

		// indices
		const uint32_t indicesCount = data.readUInt32LE();
		const uint32_t indicesOfs = data.readUInt32LE();

		// triangles
		const uint32_t trianglesCount = data.readUInt32LE();
		const uint32_t trianglesOfs = data.readUInt32LE();

		// properties (bone lookup)
		const uint32_t propertiesCount = data.readUInt32LE();
		const uint32_t propertiesOfs = data.readUInt32LE();

		// submeshes
		const uint32_t subMeshesCount = data.readUInt32LE();
		const uint32_t subMeshesOfs = data.readUInt32LE();

		// texture units (batches)
		const uint32_t textureUnitsCount = data.readUInt32LE();
		const uint32_t textureUnitsOfs = data.readUInt32LE();

		skin.bones = data.readUInt32LE();

		const size_t viewBase = data.offset();

		// read indices
		data.seek(indicesOfs + ofs);
		skin.indices = data.readUInt16LE(indicesCount);

		// read triangles
		data.seek(trianglesOfs + ofs);
		skin.triangles = data.readUInt16LE(trianglesCount);

		// read properties
		data.seek(propertiesOfs + ofs);
		skin.properties = data.readUInt8(propertiesCount);

		// read submeshes
		data.seek(subMeshesOfs + ofs);
		skin.subMeshes.resize(subMeshesCount);

		for (uint32_t i = 0; i < subMeshesCount; i++) {
			auto& sm = skin.subMeshes[i];
			sm.submeshID = data.readUInt16LE();
			sm.level = data.readUInt16LE();
			sm.vertexStart = data.readUInt16LE();
			sm.vertexCount = data.readUInt16LE();
			sm.triangleStart = data.readUInt16LE();
			sm.triangleCount = data.readUInt16LE();
			sm.boneCount = data.readUInt16LE();
			sm.boneStart = data.readUInt16LE();
			sm.boneInfluences = data.readUInt16LE();
			sm.centerBoneIndex = data.readUInt16LE();
			sm.centerPosition = data.readFloatLE(3);

			// tbc+ added sort center and radius
			if (this->version >= M2_VER_TBC_MIN) {
				sm.sortCenterPosition = data.readFloatLE(3);
				sm.sortRadius = data.readFloatLE();
			}

			sm.triangleStart += sm.level << 16;
		}

		// read texture units
		data.seek(textureUnitsOfs + ofs);
		skin.textureUnits.resize(textureUnitsCount);

		for (uint32_t i = 0; i < textureUnitsCount; i++) {
			auto& tu = skin.textureUnits[i];
			tu.flags = data.readUInt8();
			tu.priority = data.readUInt8();
			tu.shaderID = data.readUInt16LE();
			tu.skinSectionIndex = data.readUInt16LE();
			tu.flags2 = data.readUInt16LE();
			tu.colorIndex = data.readUInt16LE();
			tu.materialIndex = data.readUInt16LE();
			tu.materialLayer = data.readUInt16LE();
			tu.textureCount = data.readUInt16LE();
			tu.textureComboIndex = data.readUInt16LE();
			tu.textureCoordComboIndex = data.readUInt16LE();
			tu.textureWeightComboIndex = data.readUInt16LE();
			tu.textureTransformComboIndex = data.readUInt16LE();
		}

		skin.isLoaded = true;

		data.seek(viewBase);
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_colors(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->colors.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		this->colors[i] = {
			this->_read_m2_track(data, ofs, LegacyDataType::float3),
			this->_read_m2_track(data, ofs, LegacyDataType::int16)
		};
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_textures(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->textures.reserve(count);
	this->textureTypes.resize(count);

	for (uint32_t i = 0; i < count; i++) {
		const uint32_t textureType = this->textureTypes[i] = data.readUInt32LE();
		Texture texture(data.readUInt32LE());

		const uint32_t nameLength = data.readUInt32LE();
		const uint32_t nameOfs = data.readUInt32LE();

		// legacy textures use filename strings (store directly, not via setFileName)
		if (textureType == 0 && nameOfs > 0 && nameLength > 0) {
			const size_t pos = data.offset();
			data.seek(nameOfs + ofs);

			std::string fileName = data.readString(nameLength);
			std::erase(fileName, '\0');

			if (!fileName.empty())
				texture.fileName = fileName;

			data.seek(pos);
		}

		this->textures.push_back(std::move(texture));
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_texture_weights(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->textureWeights.resize(count);
	for (uint32_t i = 0; i < count; i++)
		this->textureWeights[i] = this->_read_m2_track(data, ofs, LegacyDataType::int16);

	data.seek(base);
}

void M2LegacyLoader::_parse_texture_transforms(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->textureTransforms.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		this->textureTransforms[i] = {
			this->_read_m2_track(data, ofs, LegacyDataType::float3),
			this->_read_m2_track(data, ofs, LegacyDataType::float4),
			this->_read_m2_track(data, ofs, LegacyDataType::float3)
		};
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_replaceable_texture_lookup(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->replaceableTextureLookup = data.readInt16LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_materials(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->materials.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		this->materials[i] = {
			data.readUInt16LE(),
			data.readUInt16LE()
		};
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_texture_combos(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->textureCombos = data.readUInt16LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_transparency_lookup(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->transparencyLookup = data.readUInt16LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_texture_transform_lookup(uint32_t ofs) {
	auto& data = this->data;
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);
	this->textureTransformsLookup = data.readUInt16LE(count);
	data.seek(base);
}

void M2LegacyLoader::_parse_bounding_box() {
	auto& data = this->data;

	this->boundingBox = {
		data.readFloatLE(3),
		data.readFloatLE(3)
	};
	this->boundingSphereRadius = data.readFloatLE();

	this->collisionBox = {
		data.readFloatLE(3),
		data.readFloatLE(3)
	};
	this->collisionSphereRadius = data.readFloatLE();
}

void M2LegacyLoader::_parse_collision(uint32_t ofs) {
	auto& data = this->data;

	const uint32_t indicesCount = data.readUInt32LE();
	const uint32_t indicesOfs = data.readUInt32LE();
	const uint32_t positionsCount = data.readUInt32LE();
	const uint32_t positionsOfs = data.readUInt32LE();
	const uint32_t normalsCount = data.readUInt32LE();
	const uint32_t normalsOfs = data.readUInt32LE();

	const size_t base = data.offset();

	// indices
	data.seek(indicesOfs + ofs);
	this->collisionIndices = data.readUInt16LE(indicesCount);

	// positions (convert z-up to y-up)
	data.seek(positionsOfs + ofs);
	this->collisionPositions.resize(positionsCount * 3);
	for (uint32_t i = 0; i < positionsCount; i++) {
		this->collisionPositions[i * 3] = data.readFloatLE();
		this->collisionPositions[i * 3 + 2] = data.readFloatLE() * -1;
		this->collisionPositions[i * 3 + 1] = data.readFloatLE();
	}

	// normals (convert z-up to y-up)
	data.seek(normalsOfs + ofs);
	this->collisionNormals.resize(normalsCount * 3);
	for (uint32_t i = 0; i < normalsCount; i++) {
		this->collisionNormals[i * 3] = data.readFloatLE();
		this->collisionNormals[i * 3 + 2] = data.readFloatLE() * -1;
		this->collisionNormals[i * 3 + 1] = data.readFloatLE();
	}

	data.seek(base);
}

void M2LegacyLoader::_parse_attachments(uint32_t ofs) {
	auto& data = this->data;

	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	this->attachments.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		this->attachments[i] = {
			data.readUInt32LE(),
			data.readUInt16LE(),
			data.readUInt16LE(),
			data.readFloatLE(3),
			this->_read_m2_track(data, ofs, LegacyDataType::uint8)
		};
	}

	data.seek(base);
}

// legacy animation block has ranges array for single-timeline
LegacyM2Track M2LegacyLoader::_read_m2_track(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType) {
	LegacyM2Track track;
	track.interpolation = data.readUInt16LE();
	track.globalSeq = data.readInt16LE();

	if (this->version < M2_VER_WOTLK) {
		// pre-wotlk: single timeline with ranges
		track.ranges = [&]() {
			auto arr = this->_read_m2_array(data, ofs, LegacyDataType::uint32_pair);
			std::vector<std::vector<uint32_t>> result;
			result.reserve(arr.size());
			for (auto& val : arr)
				result.push_back(std::get<std::vector<uint32_t>>(std::move(val)));
			return result;
		}();
		track.flatTimestamps = this->_read_m2_array(data, ofs, LegacyDataType::uint32);
		track.flatValues = this->_read_m2_array(data, ofs, dataType);
	} else {
		// wotlk: per-animation arrays
		track.nestedTimestamps = this->_read_m2_array_array(data, ofs, LegacyDataType::uint32);
		track.nestedValues = this->_read_m2_array_array(data, ofs, dataType);
	}

	return track;
}

// read simple array (for single-timeline legacy format)
std::vector<LegacyTrackValue> M2LegacyLoader::_read_m2_array(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType) {
	const uint32_t count = data.readUInt32LE();
	const uint32_t offset = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(offset + ofs);

	std::vector<LegacyTrackValue> arr(count);
	for (uint32_t i = 0; i < count; i++)
		arr[i] = this->_read_data_type(data, dataType);

	data.seek(base);
	return arr;
}

// read array of arrays (for per-animation wotlk format)
std::vector<std::vector<LegacyTrackValue>> M2LegacyLoader::_read_m2_array_array(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType) {
	const uint32_t arrCount = data.readUInt32LE();
	const uint32_t arrOfs = data.readUInt32LE();

	const size_t base = data.offset();
	data.seek(ofs + arrOfs);

	std::vector<std::vector<LegacyTrackValue>> arr(arrCount);
	for (uint32_t i = 0; i < arrCount; i++) {
		const uint32_t subArrCount = data.readUInt32LE();
		const uint32_t subArrOfs = data.readUInt32LE();
		const size_t subBase = data.offset();

		data.seek(ofs + subArrOfs);

		arr[i].resize(subArrCount);
		for (uint32_t j = 0; j < subArrCount; j++)
			arr[i][j] = this->_read_data_type(data, dataType);

		data.seek(subBase);
	}

	data.seek(base);
	return arr;
}

LegacyTrackValue M2LegacyLoader::_read_data_type(BufferWrapper& data, LegacyDataType dataType) {
	switch (dataType) {
		case LegacyDataType::uint32:
			return data.readUInt32LE();

		case LegacyDataType::uint32_pair:
			return std::vector<uint32_t>{ data.readUInt32LE(), data.readUInt32LE() };

		case LegacyDataType::int16:
			return data.readInt16LE();

		case LegacyDataType::uint8:
			return data.readUInt8();

		case LegacyDataType::float3:
			return data.readFloatLE(3);

		case LegacyDataType::float4:
			return data.readFloatLE(4);

		case LegacyDataType::compquat: {
			auto raw = data.readInt16LE(4);
			std::vector<float> result(4);
			for (int i = 0; i < 4; i++)
				result[i] = (raw[i] < 0 ? raw[i] + 32768.0f : raw[i] - 32767.0f) / 32767.0f;
			return result;
		}

		default:
			throw std::runtime_error("Unknown data type");
	}
}

std::future<LegacyM2Skin*> M2LegacyLoader::getSkin(int index) {
	return std::async(std::launch::deferred, [this, index]() -> LegacyM2Skin* {
		if (this->version < M2_VER_WOTLK) {
			// pre-wotlk: skins are already loaded inline
			if (index < 0 || static_cast<size_t>(index) >= this->skins.size())
				return nullptr;
			return &this->skins[static_cast<size_t>(index)];
		}

		// wotlk: would need external skin loading (not implemented for legacy)
		throw std::runtime_error("External skin loading not implemented for legacy WotLK M2");
	});
}

std::vector<LegacyM2Skin>& M2LegacyLoader::getSkinList() {
	return this->skins;
}

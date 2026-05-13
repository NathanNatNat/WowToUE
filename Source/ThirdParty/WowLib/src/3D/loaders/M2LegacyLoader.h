/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
*/
#pragma once

#include <cstdint>
#include <future>
#include <string>
#include <variant>
#include <vector>

class BufferWrapper;
class Texture;

// -----------------------------------------------------------------------
// M2 version constants
// -----------------------------------------------------------------------
static constexpr uint32_t MAGIC_MD20 = 0x3032444D; // 'MD20'

static constexpr uint32_t M2_VER_VANILLA_MIN = 256;
static constexpr uint32_t M2_VER_VANILLA_MAX = 257;
static constexpr uint32_t M2_VER_TBC_MIN = 260;
static constexpr uint32_t M2_VER_TBC_MAX = 263;
static constexpr uint32_t M2_VER_WOTLK = 264;

// -----------------------------------------------------------------------
// Legacy M2 track (single-timeline with ranges, or per-animation)
// -----------------------------------------------------------------------

/**
 * Legacy data type identifiers used by _read_data_type.
 */
enum class LegacyDataType {
	uint8,
	uint32,
	uint32_pair,
	int16,
	float3,
	float4,
	compquat
};

/**
 * A single track value — may be a scalar, a pair, or a float vector.
 */
using LegacyTrackValue = std::variant<uint8_t, uint32_t, int16_t, std::vector<float>, std::vector<uint32_t>>;

struct LegacyM2Track {
	int16_t globalSeq = -1;
	uint16_t interpolation = 0;

	// For pre-WotLK: single-timeline flat arrays
	// For WotLK: per-animation arrays of arrays
	// timestamps and values are stored as either:
	//   flat: vector<LegacyTrackValue>
	//   nested: vector<vector<LegacyTrackValue>>
	//   pre-WotLK: timestamps/values are flat (outer vec size = element count)
	//   WotLK: timestamps/values are nested (outer vec = per-animation)

	// Flat arrays (pre-WotLK single-timeline)
	std::vector<LegacyTrackValue> flatTimestamps;
	std::vector<LegacyTrackValue> flatValues;

	// Nested arrays (WotLK per-animation)
	std::vector<std::vector<LegacyTrackValue>> nestedTimestamps;
	std::vector<std::vector<LegacyTrackValue>> nestedValues;

	// Ranges (legacy pre-WotLK only)
	std::vector<std::vector<uint32_t>> ranges;
};

// -----------------------------------------------------------------------
// Legacy M2 animation
// -----------------------------------------------------------------------

struct LegacyM2Animation {
	uint16_t id = 0;
	uint16_t variationIndex = 0;
	uint32_t duration = 0;
	float movespeed = 0.0f;
	uint32_t flags = 0;
	int16_t frequency = 0;
	uint16_t padding = 0;
	uint32_t replayMin = 0;
	uint32_t replayMax = 0;
	std::vector<float> boxMin;
	std::vector<float> boxMax;
	float boxRadius = 0.0f;
	int16_t variationNext = 0;
	uint16_t aliasNext = 0;

	// pre-wotlk only
	uint32_t startTimestamp = 0;
	uint32_t endTimestamp = 0;
	uint32_t blendTime = 0;

	// wotlk only
	uint16_t blendTimeIn = 0;
	uint16_t blendTimeOut = 0;
};

// -----------------------------------------------------------------------
// Playable animation lookup (vanilla only)
// -----------------------------------------------------------------------

struct LegacyPlayableAnimLookup {
	uint16_t fallbackAnimationId = 0;
	uint16_t flags = 0;
};

// -----------------------------------------------------------------------
// Legacy M2 bone
// -----------------------------------------------------------------------

struct LegacyM2Bone {
	int32_t boneID = 0;
	uint32_t flags = 0;
	int16_t parentBone = -1;
	uint16_t subMeshID = 0;
	uint32_t boneNameCRC = 0; // TBC+ only

	LegacyM2Track translation;
	LegacyM2Track rotation;
	LegacyM2Track scale;
	std::vector<float> pivot;
};

// -----------------------------------------------------------------------
// Legacy M2 sub-mesh (for inline skins)
// -----------------------------------------------------------------------

struct LegacyM2SubMesh {
	uint16_t submeshID = 0;
	uint16_t level = 0;
	uint16_t vertexStart = 0;
	uint16_t vertexCount = 0;
	uint32_t triangleStart = 0;
	uint16_t triangleCount = 0;
	uint16_t boneCount = 0;
	uint16_t boneStart = 0;
	uint16_t boneInfluences = 0;
	uint16_t centerBoneIndex = 0;
	std::vector<float> centerPosition;

	// TBC+ only
	std::vector<float> sortCenterPosition;
	float sortRadius = 0.0f;
};

// -----------------------------------------------------------------------
// Legacy M2 texture unit
// -----------------------------------------------------------------------

struct LegacyM2TextureUnit {
	uint8_t flags = 0;
	uint8_t priority = 0;
	uint16_t shaderID = 0;
	uint16_t skinSectionIndex = 0;
	uint16_t flags2 = 0;
	uint16_t colorIndex = 0;
	uint16_t materialIndex = 0;
	uint16_t materialLayer = 0;
	uint16_t textureCount = 0;
	uint16_t textureComboIndex = 0;
	uint16_t textureCoordComboIndex = 0;
	uint16_t textureWeightComboIndex = 0;
	uint16_t textureTransformComboIndex = 0;
};

// -----------------------------------------------------------------------
// Legacy M2 skin (inline view)
// -----------------------------------------------------------------------

struct LegacyM2Skin {
	std::vector<uint16_t> indices;
	std::vector<uint16_t> triangles;
	std::vector<uint8_t> properties;
	std::vector<LegacyM2SubMesh> subMeshes;
	std::vector<LegacyM2TextureUnit> textureUnits;
	uint32_t bones = 0;
	bool isLoaded = false;
};

// -----------------------------------------------------------------------
// Legacy M2 color
// -----------------------------------------------------------------------

struct LegacyM2Color {
	LegacyM2Track color;
	LegacyM2Track alpha;
};

// -----------------------------------------------------------------------
// Legacy M2 texture transform
// -----------------------------------------------------------------------

struct LegacyM2TextureTransform {
	LegacyM2Track translation;
	LegacyM2Track rotation;
	LegacyM2Track scaling;
};

// -----------------------------------------------------------------------
// Legacy M2 material
// -----------------------------------------------------------------------

struct LegacyM2Material {
	uint16_t flags = 0;
	uint16_t blendingMode = 0;
};

// -----------------------------------------------------------------------
// Bounding box
// -----------------------------------------------------------------------

struct LegacyBoundingBox {
	std::vector<float> min;
	std::vector<float> max;
};

// -----------------------------------------------------------------------
// Legacy M2 attachment
// -----------------------------------------------------------------------

struct LegacyM2Attachment {
	uint32_t id = 0;
	uint16_t bone = 0;
	uint16_t unknown = 0;
	std::vector<float> position;
	LegacyM2Track animateAttached;
};

// -----------------------------------------------------------------------
// M2LegacyLoader class
// -----------------------------------------------------------------------

class M2LegacyLoader {
public:
	/**
	 * Construct a new M2LegacyLoader instance.
	 * @param data
	 */
	explicit M2LegacyLoader(BufferWrapper& data);

	/**
	 * Load the M2 model.
	 */
	std::future<void> load();

	/**
	 * Get a skin by index.
	 * @param index
	 */
	std::future<LegacyM2Skin*> getSkin(int index);

	/**
	 * Get all skins.
	 */
	std::vector<LegacyM2Skin>& getSkinList();

	bool isLoaded = false;
	uint32_t version = 0;
	std::string name;
	uint32_t flags = 0;
	std::vector<uint32_t> globalLoops;
	std::vector<LegacyM2Animation> animations;
	std::vector<int16_t> animationLookup;
	std::vector<LegacyPlayableAnimLookup> playableAnimationLookup;
	std::vector<LegacyM2Bone> bones;
	uint32_t viewCount = 0;
	std::vector<LegacyM2Skin> skins;
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<float> uv;
	std::vector<float> uv2;
	std::vector<uint8_t> boneWeights;
	std::vector<uint8_t> boneIndices;
	std::vector<LegacyM2Color> colors;
	std::vector<Texture> textures;
	std::vector<uint32_t> textureTypes;
	std::vector<LegacyM2Track> textureWeights;
	std::vector<LegacyM2TextureTransform> textureTransforms;
	std::vector<int16_t> replaceableTextureLookup;
	std::vector<LegacyM2Material> materials;
	std::vector<uint16_t> textureCombos;
	std::vector<uint16_t> transparencyLookup;
	std::vector<uint16_t> textureTransformsLookup;

	LegacyBoundingBox boundingBox;
	float boundingSphereRadius = 0.0f;
	LegacyBoundingBox collisionBox;
	float collisionSphereRadius = 0.0f;

	std::vector<uint16_t> collisionIndices;
	std::vector<float> collisionPositions;
	std::vector<float> collisionNormals;

	std::vector<LegacyM2Attachment> attachments;

private:
	BufferWrapper& data;

	void _parse_header();
	void _parse_model_name(uint32_t ofs);
	void _parse_global_loops(uint32_t ofs);
	void _parse_animations(uint32_t ofs);
	void _parse_animation_lookup(uint32_t ofs);
	void _parse_playable_animation_lookup(uint32_t ofs);
	void _parse_bones(uint32_t ofs);
	void _parse_vertices(uint32_t ofs);
	void _parse_views_inline(uint32_t ofs);
	void _parse_colors(uint32_t ofs);
	void _parse_textures(uint32_t ofs);
	void _parse_texture_weights(uint32_t ofs);
	void _parse_texture_transforms(uint32_t ofs);
	void _parse_replaceable_texture_lookup(uint32_t ofs);
	void _parse_materials(uint32_t ofs);
	void _parse_texture_combos(uint32_t ofs);
	void _parse_transparency_lookup(uint32_t ofs);
	void _parse_texture_transform_lookup(uint32_t ofs);
	void _parse_bounding_box();
	void _parse_collision(uint32_t ofs);
	void _parse_attachments(uint32_t ofs);

	LegacyM2Track _read_m2_track(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType);
	std::vector<LegacyTrackValue> _read_m2_array(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType);
	std::vector<std::vector<LegacyTrackValue>> _read_m2_array_array(BufferWrapper& data, uint32_t ofs, LegacyDataType dataType);
	LegacyTrackValue _read_data_type(BufferWrapper& data, LegacyDataType dataType);
	void _convert_bone_coords(LegacyM2Bone& bone);
};

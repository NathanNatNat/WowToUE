/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "M2Generics.h"

class BufferWrapper;
class Skin;
class Texture;

// -----------------------------------------------------------------------
// M2 sub-structures
// -----------------------------------------------------------------------

struct M2Animation {
	uint16_t id = 0;
	uint16_t variationIndex = 0;
	uint32_t duration = 0;
	float movespeed = 0.0f;
	uint32_t flags = 0;
	int16_t frequency = 0;
	uint16_t padding = 0;
	uint32_t replayMin = 0;
	uint32_t replayMax = 0;
	uint16_t blendTimeIn = 0;
	uint16_t blendTimeOut = 0;
	std::vector<float> boxPosMin;
	std::vector<float> boxPosMax;
	float boxRadius = 0.0f;
	int16_t variationNext = 0;
	uint16_t aliasNext = 0;
};

struct M2Bone {
	int32_t boneID = 0;
	uint32_t flags = 0;
	int16_t parentBone = -1;
	uint16_t subMeshID = 0;
	uint32_t boneNameCRC = 0;
	M2Track translation;
	M2Track rotation;
	M2Track scale;
	std::vector<float> pivot;
};

struct M2AnimFileEntry {
	uint16_t animID = 0;
	uint16_t subAnimID = 0;
	uint32_t fileDataID = 0;
};

struct M2Material {
	uint16_t flags = 0;
	uint16_t blendingMode = 0;
};

struct M2Color {
	M2Track color;
	M2Track alpha;
};

struct M2TextureTransform {
	M2Track translation;
	M2Track rotation;
	M2Track scaling;
};

struct M2Attachment {
	uint32_t id = 0;
	uint16_t bone = 0;
	uint16_t unknown = 0;
	std::vector<float> position;
	M2Track animateAttached;
};

// -----------------------------------------------------------------------
// M2Loader class
// -----------------------------------------------------------------------

class M2Loader {
public:
	/**
	 * Construct a new M2Loader instance.
	 * @param data
	 */
	explicit M2Loader(BufferWrapper& data);

	/**
	 * Load the M2 model.
	 */
	std::future<void> load();

	/**
	 * Get a skin at a given index from this->skins.
	 * @param index
	 */
	std::future<Skin*> getSkin(uint32_t index);

	/**
	 * Returns the internal array of Skin objects.
	 * Note: Unlike getSkin(), this does not load any of the skins.
	 */
	std::vector<Skin>& getSkinList();

	/**
	 * Load and apply .anim files to loaded M2 model.
	 */
	std::future<void> loadAnims(bool load_all = true);

	/**
	 * Load .anim file for a specific animation index (lazy loading).
	 * @param animationIndex
	 * @returns true if loaded successfully, false otherwise
	 */
	std::future<bool> loadAnimsForIndex(uint32_t animationIndex);

	/**
	 * Get attachment by attachment ID (e.g., 11 for helmet).
	 * @param attachmentId
	 * @returns pointer to attachment, or nullptr if not found
	 */
	const M2Attachment* getAttachmentById(uint32_t attachmentId) const;

	bool isLoaded = false;
	std::map<uint32_t, BufferWrapper*> animFiles;

	uint32_t version = 0;
	uint32_t flags = 0;
	std::string name;

	std::vector<int32_t> globalLoops;
	std::vector<M2Animation> animations;
	std::vector<int16_t> animationLookup;
	std::vector<M2Bone> bones;
	uint32_t viewCount = 0;
	std::vector<float> vertices;
	std::vector<float> normals;
	std::vector<float> uv;
	std::vector<float> uv2;
	std::vector<uint8_t> boneWeights;
	std::vector<uint8_t> boneIndices;
	std::vector<Skin> skins;
	std::vector<Skin> lodSkins;
	std::vector<M2Color> colors;
	std::vector<Texture> textures;
	std::vector<uint32_t> textureTypes;
	std::vector<M2Track> textureWeights;
	std::vector<M2TextureTransform> textureTransforms;
	std::vector<int16_t> replaceableTextureLookup;
	std::vector<M2Material> materials;
	std::vector<uint16_t> textureCombos;
	std::vector<uint16_t> transparencyLookup;
	std::vector<uint16_t> textureTransformsLookup;

	CAaBB boundingBox;
	float boundingSphereRadius = 0.0f;
	CAaBB collisionBox;
	float collisionSphereRadius = 0.0f;

	std::vector<uint16_t> collisionIndices;
	std::vector<float> collisionPositions;
	std::vector<float> collisionNormals;

	std::vector<M2Attachment> attachments;
	std::vector<int16_t> attachmentLookup;

	std::vector<M2AnimFileEntry> animFileIDs;
	std::vector<uint32_t> boneFileIDs;
	uint32_t skeletonFileID = 0;

private:
	BufferWrapper& data;
	uint32_t md21Ofs = 0;
	bool md21Parsed = false;

	// Owned anim buffers
	std::vector<std::unique_ptr<BufferWrapper>> ownedAnimBuffers;

	void _patch_bone_animation(uint32_t animIndex);

	void parseChunk_SFID(uint32_t chunkSize);
	void parseChunk_TXID();
	void parseChunk_SKID();
	void parseChunk_BFID(uint32_t chunkSize);
	void parseChunk_AFID(uint32_t chunkSize);
	std::future<void> parseChunk_MD21();

	void parseChunk_MD21_modelName(uint32_t ofs);
	void parseChunk_MD21_globalLoops(uint32_t ofs);
	void parseChunk_MD21_animations(uint32_t ofs);
	void parseChunk_MD21_animationLookup(uint32_t ofs);
	void parseChunk_MD21_bones(uint32_t ofs);
	void parseChunk_MD21_vertices(uint32_t ofs);
	void parseChunk_MD21_colors(uint32_t ofs);
	void parseChunk_MD21_textures(uint32_t ofs);
	void parseChunk_MD21_textureWeights(uint32_t ofs);
	void parseChunk_MD21_textureTransforms(uint32_t ofs);
	void parseChunk_MD21_replaceableTextureLookup(uint32_t ofs);
	void parseChunk_MD21_materials(uint32_t ofs);
	void parseChunk_MD21_textureCombos(uint32_t ofs);
	void parseChunk_MD21_transparencyLookup(uint32_t ofs);
	void parseChunk_MD21_textureTransformLookup(uint32_t ofs);
	void parseChunk_MD21_collision(uint32_t ofs);
	void parseChunk_MD21_attachments(uint32_t ofs);
	void parseChunk_MD21_attachmentLookup(uint32_t ofs);
};

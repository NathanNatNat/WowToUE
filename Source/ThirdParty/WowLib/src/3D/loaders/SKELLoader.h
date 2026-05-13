/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "M2Generics.h"

class BufferWrapper;

// -----------------------------------------------------------------------
// SKEL sub-structures
// -----------------------------------------------------------------------

struct SKELAnimation {
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

struct SKELBone {
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

struct SKELAnimFileEntry {
	uint16_t animID = 0;
	uint16_t subAnimID = 0;
	uint32_t fileDataID = 0;
};

struct SKELAttachment {
	uint32_t id = 0;
	uint16_t bone = 0;
	uint16_t unknown = 0;
	std::vector<float> position;
	M2Track animateAttached;
};

// -----------------------------------------------------------------------
// SKELLoader class
// -----------------------------------------------------------------------

// See: https://wowdev.wiki/M2/.skel
class SKELLoader {
public:
	/**
	 * Construct a new SKELLoader instance.
	 * @param data
	 */
	explicit SKELLoader(BufferWrapper& data);

	/**
	 * Load the skeleton file.
	 */
	void load();

	/**
	 * Load .anim file for a specific animation index (lazy loading).
	 * @param animation_index
	 * @returns true if loaded successfully
	 */
	bool loadAnimsForIndex(uint32_t animation_index);

	/**
	 * Load and apply all .anim files.
	 */
	void loadAnims(bool load_all = true);

	/**
	 * Get attachment by attachment ID (e.g., 11 for helmet).
	 * @param attachmentId
	 * @returns pointer to attachment, or nullptr if not found
	 */
	const SKELAttachment* getAttachmentById(uint32_t attachmentId) const;

	bool isLoaded = false;
	std::map<uint32_t, BufferWrapper*> animFiles;

	uint32_t parent_skel_file_id = 0;
	size_t boneOffset = 0;

	std::vector<int32_t> globalLoops;
	std::vector<SKELAnimation> animations;
	std::vector<int16_t> animationLookup;
	std::vector<SKELBone> bones;
	std::vector<SKELAnimFileEntry> animFileIDs;
	std::vector<uint32_t> boneFileIDs;

	std::vector<SKELAttachment> attachments;
	std::vector<int16_t> attachmentLookup;

private:
	static constexpr uint32_t CHUNK_SKB1 = 0x31424B53;
	static constexpr uint32_t CHUNK_SKPD = 0x44504B53;
	static constexpr uint32_t CHUNK_SKS1 = 0x31534B53;
	static constexpr uint32_t CHUNK_SKA1 = 0x31414B53;
	static constexpr uint32_t CHUNK_AFID = 0x44494641;
	static constexpr uint32_t CHUNK_BFID = 0x44494642;

	BufferWrapper& data;

	// Owned anim buffers
	std::vector<std::unique_ptr<BufferWrapper>> ownedAnimBuffers;

	void parse_chunk_skpd();
	void parse_chunk_skb1();
	void parse_chunk_sks1();
	void parse_chunk_ska1();
	void parse_chunk_afid(uint32_t chunkSize);
	void parse_chunk_bfid(uint32_t chunkSize);

	void _patch_bone_animation(uint32_t animIndex);
};

#include "SKELLoader.h"
#include "M2Generics.h"
#include "ANIMLoader.h"
#include "../AnimMapper.h"
#include "../../buffer.h"
#include "../../core.h"
#include "../../casc/casc-source.h"
#include "../../log.h"

#include <format>
#include <stdexcept>

// See: https://wowdev.wiki/M2/.skel
SKELLoader::SKELLoader(BufferWrapper& data)
	: isLoaded(false), data(data) {
}

/**
 * Load the skeleton file.
 */
void SKELLoader::load() {
	// Prevent multiple loading of the same file.
	if (this->isLoaded == true)
		return;

	// defer SKB1/SKA1 parsing until after SKS1 so this->animations is available
	int64_t skb1Ofs = -1;
	int64_t ska1Ofs = -1;

	while (this->data.remainingBytes() > 0) {
		const uint32_t chunkID = this->data.readUInt32LE();
		const uint32_t chunkSize = this->data.readUInt32LE();
		const size_t nextChunkPos = this->data.offset() + chunkSize;

		switch (chunkID) {
			case CHUNK_SKB1: skb1Ofs = static_cast<int64_t>(this->data.offset()); break;
			case CHUNK_SKA1: ska1Ofs = static_cast<int64_t>(this->data.offset()); break;
			case CHUNK_SKPD: this->parse_chunk_skpd(); break;
			case CHUNK_SKS1: this->parse_chunk_sks1(); break;
			case CHUNK_AFID: this->parse_chunk_afid(chunkSize); break;
			case CHUNK_BFID: this->parse_chunk_bfid(chunkSize); break;
		}

		this->data.seek(nextChunkPos);
	}

	// parse bones/attachments after animations are available
	if (skb1Ofs >= 0) {
		this->data.seek(static_cast<size_t>(skb1Ofs));
		this->parse_chunk_skb1();
	}

	if (ska1Ofs >= 0) {
		this->data.seek(static_cast<size_t>(ska1Ofs));
		this->parse_chunk_ska1();
	}

	this->isLoaded = true;
}

void SKELLoader::parse_chunk_skpd() {
	this->data.move(8); // _0x00[8]
	this->parent_skel_file_id = this->data.readUInt32LE();
	this->data.move(4); // _0x0c[4]
}

void SKELLoader::parse_chunk_skb1() {
	auto& d = this->data;
	const size_t chunk_ofs = d.offset();
	this->boneOffset = d.offset();

	const uint32_t bone_count = d.readUInt32LE();
	const uint32_t bone_ofs = d.readUInt32LE();

	const size_t base_ofs = d.offset();
	d.seek(chunk_ofs + bone_ofs);

	std::vector<M2Sequence> sequences(this->animations.size());
	for (size_t i = 0; i < this->animations.size(); i++)
		sequences[i] = { this->animations[i].flags };

	// store offsets for lazy .anim patching
	this->bones.resize(bone_count);
	for (uint32_t i = 0; i < bone_count; i++) {
		SKELBone bone;
		bone.boneID = d.readInt32LE();
		bone.flags = d.readUInt32LE();
		bone.parentBone = d.readInt16LE();
		bone.subMeshID = d.readUInt16LE();
		bone.boneNameCRC = d.readUInt32LE();
		bone.translation = read_m2_track(d, static_cast<uint32_t>(chunk_ofs), M2DataType::float3, false, this->animFiles, true, &sequences);
		bone.rotation = read_m2_track(d, static_cast<uint32_t>(chunk_ofs), M2DataType::compquat, false, this->animFiles, true, &sequences);
		bone.scale = read_m2_track(d, static_cast<uint32_t>(chunk_ofs), M2DataType::float3, false, this->animFiles, true, &sequences);
		bone.pivot = d.readFloatLE(3);

		// Convert bone transformations coordinate system.
		auto& translations = bone.translation.values;
		auto& rotations = bone.rotation.values;
		auto& scale = bone.scale.values;
		auto& pivot = bone.pivot;

		for (size_t ai = 0; ai < translations.size(); ai++) {
			for (size_t j = 0; j < translations[ai].size(); j++) {
				auto& v = std::get<std::vector<float>>(translations[ai][j]);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy * -1;
				v[1] = dz;
			}
		}

		for (size_t ai = 0; ai < rotations.size(); ai++) {
			for (size_t j = 0; j < rotations[ai].size(); j++) {
				auto& v = std::get<std::vector<float>>(rotations[ai][j]);
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

		for (size_t ai = 0; ai < scale.size(); ai++) {
			for (size_t j = 0; j < scale[ai].size(); j++) {
				auto& v = std::get<std::vector<float>>(scale[ai][j]);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy;
				v[1] = dz;
			}
		}

		{
			const float pivotX = pivot[0];
			const float pivotY = pivot[1];
			const float pivotZ = pivot[2];
			pivot[0] = pivotX;
			pivot[2] = pivotY * -1;
			pivot[1] = pivotZ;
		}

		this->bones[i] = std::move(bone);
	}

	d.seek(base_ofs);
}

void SKELLoader::parse_chunk_sks1() {
	// Global loops
	const size_t chunk_ofs = this->data.offset();

	const uint32_t globalLoopCount = this->data.readUInt32LE();
	const uint32_t globalLoopOfs = this->data.readUInt32LE();

	size_t prevPos = this->data.offset();
	this->data.seek(globalLoopOfs + chunk_ofs);

	this->globalLoops = this->data.readInt32LE(globalLoopCount);

	this->data.seek(prevPos);

	// Sequences
	const uint32_t animationCount = this->data.readUInt32LE();
	const uint32_t animationOfs = this->data.readUInt32LE();

	prevPos = this->data.offset();
	this->data.seek(animationOfs + chunk_ofs);

	this->animations.resize(animationCount);
	for (uint32_t i = 0; i < animationCount; i++) {
		SKELAnimation& anim = this->animations[i];
		anim.id = this->data.readUInt16LE();
		anim.variationIndex = this->data.readUInt16LE();
		anim.duration = this->data.readUInt32LE();
		anim.movespeed = this->data.readFloatLE();
		anim.flags = this->data.readUInt32LE();
		anim.frequency = this->data.readInt16LE();
		anim.padding = this->data.readUInt16LE();
		anim.replayMin = this->data.readUInt32LE();
		anim.replayMax = this->data.readUInt32LE();
		anim.blendTimeIn = this->data.readUInt16LE();
		anim.blendTimeOut = this->data.readUInt16LE();
		anim.boxPosMin = this->data.readFloatLE(3);
		anim.boxPosMax = this->data.readFloatLE(3);
		anim.boxRadius = this->data.readFloatLE();
		anim.variationNext = this->data.readInt16LE();
		anim.aliasNext = this->data.readUInt16LE();
	}

	this->data.seek(prevPos);

	// Sequence lookups
	const uint32_t animationLookupCount = this->data.readUInt32LE();
	const uint32_t animationLookupOfs = this->data.readUInt32LE();

	prevPos = this->data.offset();
	this->data.seek(animationLookupOfs + chunk_ofs);

	this->animationLookup = this->data.readInt16LE(animationLookupCount);

	this->data.seek(prevPos);

	// Unused spot (for now)
	this->data.move(8);
}

/**
 * Parse SKA1 chunk for attachments.
 */
void SKELLoader::parse_chunk_ska1() {
	const size_t chunk_ofs = this->data.offset();

	// attachments array
	const uint32_t attachmentCount = this->data.readUInt32LE();
	const uint32_t attachmentOfs = this->data.readUInt32LE();

	// attachment lookup array
	const uint32_t attachmentLookupCount = this->data.readUInt32LE();
	const uint32_t attachmentLookupOfs = this->data.readUInt32LE();

	std::vector<M2Sequence> sequences(this->animations.size());
	for (size_t i = 0; i < this->animations.size(); i++)
		sequences[i] = { this->animations[i].flags };

	// parse attachments
	size_t prevPos = this->data.offset();
	this->data.seek(attachmentOfs + chunk_ofs);

	this->attachments.resize(attachmentCount);
	for (uint32_t i = 0; i < attachmentCount; i++) {
		SKELAttachment& att = this->attachments[i];
		att.id = this->data.readUInt32LE();
		att.bone = this->data.readUInt16LE();
		att.unknown = this->data.readUInt16LE();
		att.position = this->data.readFloatLE(3);
		std::map<uint32_t, BufferWrapper*> emptyAnimFiles;
		att.animateAttached = read_m2_track(this->data, static_cast<uint32_t>(chunk_ofs), M2DataType::uint8, false, emptyAnimFiles, false, &sequences);
	}

	this->data.seek(prevPos);

	// parse attachment lookup
	prevPos = this->data.offset();
	this->data.seek(attachmentLookupOfs + chunk_ofs);

	this->attachmentLookup = this->data.readInt16LE(attachmentLookupCount);

	this->data.seek(prevPos);
}

/**
 * Get attachment by attachment ID (e.g., 11 for helmet).
 */
const SKELAttachment* SKELLoader::getAttachmentById(uint32_t attachmentId) const {
	if (this->attachmentLookup.empty() || attachmentId >= this->attachmentLookup.size())
		return nullptr;

	const int16_t index = this->attachmentLookup[attachmentId];
	if (index < 0 || static_cast<size_t>(index) >= this->attachments.size())
		return nullptr;

	return &this->attachments[static_cast<size_t>(index)];
}

/**
 * Parse AFID chunk for .anim file data IDs.
 */
void SKELLoader::parse_chunk_afid(uint32_t chunkSize) {
	const uint32_t entryCount = chunkSize / 8;
	this->animFileIDs.resize(entryCount);

	for (uint32_t i = 0; i < entryCount; i++) {
		SKELAnimFileEntry& entry = this->animFileIDs[i];
		entry.animID = this->data.readUInt16LE();
		entry.subAnimID = this->data.readUInt16LE();
		entry.fileDataID = this->data.readUInt32LE();
	}
}

/**
 * Parse BFID chunk for .bone file data IDs.
 */
void SKELLoader::parse_chunk_bfid(uint32_t chunkSize) {
	this->boneFileIDs = this->data.readUInt32LE(chunkSize / 4);
}

bool SKELLoader::loadAnimsForIndex(uint32_t animation_index) {
	if (this->animFiles.count(animation_index))
		return true;

	SKELAnimation* animation = &this->animations[animation_index];

	if ((animation->flags & 0x40) == 0x40) {
		while ((animation->flags & 0x40) == 0x40)
			animation = &this->animations[animation->aliasNext];
	}

	if (this->animFileIDs.empty())
		return false;

	for (const auto& entry : this->animFileIDs) {
		if (entry.animID != animation->id || entry.subAnimID != animation->variationIndex)
			continue;

		const uint32_t fileDataID = entry.fileDataID;
		if (fileDataID == 0)
			return false;

		logging::write(std::format("lazy load .anim for {} ({}) sub={} fileDataID={}", entry.animID, get_anim_name(entry.animID), entry.subAnimID, fileDataID));

		BufferWrapper animData = core::view->casc->getVirtualFileByID(fileDataID);
		auto ownedBuf = std::make_unique<BufferWrapper>(std::move(animData));
		BufferWrapper* bufPtr = ownedBuf.get();
		this->ownedAnimBuffers.push_back(std::move(ownedBuf));

		auto loader = std::make_unique<ANIMLoader>(*bufPtr);
		loader->load(true);

		if (loader->hasSkeletonBoneData) {
			auto parsedBuf = std::make_unique<BufferWrapper>(std::move(loader->skeletonBoneData));
			BufferWrapper* parsedPtr = parsedBuf.get();
			this->ownedAnimBuffers.push_back(std::move(parsedBuf));
			this->animFiles[animation_index] = parsedPtr;
		} else {
			auto parsedBuf = std::make_unique<BufferWrapper>(std::move(loader->animData));
			BufferWrapper* parsedPtr = parsedBuf.get();
			this->ownedAnimBuffers.push_back(std::move(parsedBuf));
			this->animFiles[animation_index] = parsedPtr;
		}

		this->_patch_bone_animation(animation_index);

		return true;
	}

	return false;
}

/**
 * Patch bone animation data for a specific animation index.
 */
void SKELLoader::_patch_bone_animation(uint32_t animIndex) {
	auto it = this->animFiles.find(animIndex);
	if (it == this->animFiles.end() || !it->second || this->bones.empty())
		return;

	BufferWrapper& animBuffer = *it->second;

	for (auto& bone : this->bones) {
		patch_track_animation(bone.translation, animIndex, animBuffer, M2DataType::float3);
		patch_track_animation(bone.rotation, animIndex, animBuffer, M2DataType::compquat);
		patch_track_animation(bone.scale, animIndex, animBuffer, M2DataType::float3);

		// apply coordinate system conversion to patched data
		if (animIndex < bone.translation.values.size()) {
			auto& translations = bone.translation.values[animIndex];
			for (size_t j = 0; j < translations.size(); j++) {
				auto& v = std::get<std::vector<float>>(translations[j]);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy * -1;
				v[1] = dz;
			}
		}

		if (animIndex < bone.rotation.values.size()) {
			auto& rotations = bone.rotation.values[animIndex];
			for (size_t j = 0; j < rotations.size(); j++) {
				auto& v = std::get<std::vector<float>>(rotations[j]);
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

		if (animIndex < bone.scale.values.size()) {
			auto& scaleVals = bone.scale.values[animIndex];
			for (size_t j = 0; j < scaleVals.size(); j++) {
				auto& v = std::get<std::vector<float>>(scaleVals[j]);
				const float dx = v[0];
				const float dy = v[1];
				const float dz = v[2];
				v[0] = dx;
				v[2] = dy;
				v[1] = dz;
			}
		}
	}
}

void SKELLoader::loadAnims(bool load_all) {
	if (!load_all)
		return;

	for (size_t i = 0; i < this->animations.size(); i++) {
		SKELAnimation* animation = &this->animations[i];

		// if animation is an alias, resolve it
		if ((animation->flags & 0x40) == 0x40) {
			while ((animation->flags & 0x40) == 0x40)
				animation = &this->animations[animation->aliasNext];
		}

		if ((animation->flags & 0x20) == 0x20) {
			logging::write("Skipping .anim loading for " + get_anim_name(animation->id) + " because it should be in SKEL");
			continue;
		}

		for (const auto& entry : this->animFileIDs) {
			if (entry.animID != animation->id || entry.subAnimID != animation->variationIndex)
				continue;

			const uint32_t fileDataID = entry.fileDataID;
			if (!this->animFiles.count(static_cast<uint32_t>(i))) {
				if (fileDataID == 0) {
					logging::write("Skipping .anim loading for " + get_anim_name(entry.animID) + " because it has no fileDataID");
					continue;
				}

				logging::write(std::format("Loading .anim file for animation: {} ({}) - {}", entry.animID, get_anim_name(entry.animID), entry.subAnimID));

				BufferWrapper animData = core::view->casc->getVirtualFileByID(fileDataID);
				auto ownedBuf = std::make_unique<BufferWrapper>(std::move(animData));
				BufferWrapper* bufPtr = ownedBuf.get();
				this->ownedAnimBuffers.push_back(std::move(ownedBuf));

				auto loader = std::make_unique<ANIMLoader>(*bufPtr);
				loader->load(true);

				if (loader->hasSkeletonBoneData) {
					auto parsedBuf = std::make_unique<BufferWrapper>(std::move(loader->skeletonBoneData));
					BufferWrapper* parsedPtr = parsedBuf.get();
					this->ownedAnimBuffers.push_back(std::move(parsedBuf));
					this->animFiles[static_cast<uint32_t>(i)] = parsedPtr;
				} else {
					auto parsedBuf = std::make_unique<BufferWrapper>(std::move(loader->animData));
					BufferWrapper* parsedPtr = parsedBuf.get();
					this->ownedAnimBuffers.push_back(std::move(parsedBuf));
					this->animFiles[static_cast<uint32_t>(i)] = parsedPtr;
				}

				this->_patch_bone_animation(static_cast<uint32_t>(i));
			}
		}

		if (!this->animFiles.count(static_cast<uint32_t>(i)))
			logging::write("Failed to load .anim file for animation: " + std::to_string(animation->id) + " (" + get_anim_name(animation->id) + ") - " + std::to_string(animation->variationIndex));
	}
}
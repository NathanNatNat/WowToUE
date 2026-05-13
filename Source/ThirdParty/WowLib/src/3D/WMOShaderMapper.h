/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <cstdint>
#include <unordered_map>

namespace wmo_shader_mapper {

enum class WMOVertexShader : int {
	None = -1,
	MapObjDiffuse_T1 = 0,
	MapObjDiffuse_T1_Refl = 1,
	MapObjDiffuse_T1_Env_T2 = 2,
	MapObjSpecular_T1 = 3,
	MapObjDiffuse_Comp = 4,
	MapObjDiffuse_Comp_Refl = 5,
	MapObjDiffuse_Comp_Terrain = 6,
	MapObjDiffuse_CompAlpha = 7,
	MapObjParallax = 8
};

enum class WMOPixelShader : int {
	None = -1,
	MapObjDiffuse = 0,
	MapObjSpecular = 1,
	MapObjMetal = 2,
	MapObjEnv = 3,
	MapObjOpaque = 4,
	MapObjEnvMetal = 5,
	MapObjTwoLayerDiffuse = 6,
	MapObjTwoLayerEnvMetal = 7,
	MapObjTwoLayerTerrain = 8,
	MapObjDiffuseEmissive = 9,
	MapObjMaskedEnvMetal = 10,
	MapObjEnvMetalEmissive = 11,
	MapObjTwoLayerDiffuseOpaque = 12,
	MapObjTwoLayerDiffuseEmissive = 13,
	MapObjAdditiveMaskedEnvMetal = 14,
	MapObjTwoLayerDiffuseMod2x = 15,
	MapObjTwoLayerDiffuseMod2xNA = 16,
	MapObjTwoLayerDiffuseAlpha = 17,
	MapObjLod = 18,
	MapObjParallax = 19,
	MapObjDFShader = 20
};

enum class MOMTShader : int {
	Diffuse = 0,
	Specular = 1,
	Metal = 2,
	Env = 3,
	Opaque = 4,
	EnvMetal = 5,
	TwoLayerDiffuse = 6,
	TwoLayerEnvMetal = 7,
	TwoLayerTerrain = 8,
	DiffuseEmissive = 9,
	WaterWindow = 10,
	MaskedEnvMetal = 11,
	EnvMetalEmissive = 12,
	TwoLayerDiffuseOpaque = 13,
	SubmarineWindow = 14,
	TwoLayerDiffuseEmissive = 15,
	DiffuseTerrain = 16,
	AdditiveMaskedEnvMetal = 17,
	TwoLayerDiffuseMod2x = 18,
	TwoLayerDiffuseMod2xNA = 19,
	TwoLayerDiffuseAlpha = 20,
	Lod = 21,
	Parallax = 22,
	DF_MoreTexture_Unknown = 23
};

struct WMOShaderEntry {
	WMOVertexShader VertexShader;
	WMOPixelShader PixelShader;
};

extern const std::unordered_map<int, WMOShaderEntry> WMOShaderMap;

} // namespace wmo_shader_mapper

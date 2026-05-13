#include "WMOShaderMapper.h"

namespace wmo_shader_mapper {

const std::unordered_map<int, WMOShaderEntry> WMOShaderMap = {
	{static_cast<int>(MOMTShader::Diffuse),                 {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjDiffuse}},
	{static_cast<int>(MOMTShader::Specular),                {WMOVertexShader::MapObjSpecular_T1,         WMOPixelShader::MapObjSpecular}},
	{static_cast<int>(MOMTShader::Metal),                   {WMOVertexShader::MapObjSpecular_T1,         WMOPixelShader::MapObjMetal}},
	{static_cast<int>(MOMTShader::Env),                     {WMOVertexShader::MapObjDiffuse_T1_Refl,     WMOPixelShader::MapObjEnv}},
	{static_cast<int>(MOMTShader::Opaque),                  {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjOpaque}},
	{static_cast<int>(MOMTShader::EnvMetal),                {WMOVertexShader::MapObjDiffuse_T1_Refl,     WMOPixelShader::MapObjEnvMetal}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuse),         {WMOVertexShader::MapObjDiffuse_Comp,        WMOPixelShader::MapObjTwoLayerDiffuse}},
	{static_cast<int>(MOMTShader::TwoLayerEnvMetal),        {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjTwoLayerEnvMetal}},
	{static_cast<int>(MOMTShader::TwoLayerTerrain),         {WMOVertexShader::MapObjDiffuse_Comp_Terrain, WMOPixelShader::MapObjTwoLayerTerrain}},
	{static_cast<int>(MOMTShader::DiffuseEmissive),         {WMOVertexShader::MapObjDiffuse_Comp,        WMOPixelShader::MapObjDiffuseEmissive}},
	{static_cast<int>(MOMTShader::WaterWindow),             {WMOVertexShader::None,                      WMOPixelShader::None}},
	{static_cast<int>(MOMTShader::MaskedEnvMetal),          {WMOVertexShader::MapObjDiffuse_T1_Env_T2,   WMOPixelShader::MapObjMaskedEnvMetal}},
	{static_cast<int>(MOMTShader::EnvMetalEmissive),        {WMOVertexShader::MapObjDiffuse_T1_Env_T2,   WMOPixelShader::MapObjEnvMetalEmissive}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuseOpaque),   {WMOVertexShader::MapObjDiffuse_Comp,        WMOPixelShader::MapObjTwoLayerDiffuseOpaque}},
	{static_cast<int>(MOMTShader::SubmarineWindow),         {WMOVertexShader::None,                      WMOPixelShader::None}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuseEmissive), {WMOVertexShader::MapObjDiffuse_Comp,        WMOPixelShader::MapObjTwoLayerDiffuseEmissive}},
	{static_cast<int>(MOMTShader::DiffuseTerrain),          {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjDiffuse}},
	{static_cast<int>(MOMTShader::AdditiveMaskedEnvMetal),  {WMOVertexShader::MapObjDiffuse_T1_Env_T2,   WMOPixelShader::MapObjAdditiveMaskedEnvMetal}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuseMod2x),    {WMOVertexShader::MapObjDiffuse_CompAlpha,   WMOPixelShader::MapObjTwoLayerDiffuseMod2x}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuseMod2xNA),  {WMOVertexShader::MapObjDiffuse_Comp,        WMOPixelShader::MapObjTwoLayerDiffuseMod2xNA}},
	{static_cast<int>(MOMTShader::TwoLayerDiffuseAlpha),    {WMOVertexShader::MapObjDiffuse_CompAlpha,   WMOPixelShader::MapObjTwoLayerDiffuseAlpha}},
	{static_cast<int>(MOMTShader::Lod),                     {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjLod}},
	{static_cast<int>(MOMTShader::Parallax),                {WMOVertexShader::MapObjParallax,            WMOPixelShader::MapObjParallax}},
	{static_cast<int>(MOMTShader::DF_MoreTexture_Unknown),  {WMOVertexShader::MapObjDiffuse_T1,          WMOPixelShader::MapObjDFShader}}
};

}

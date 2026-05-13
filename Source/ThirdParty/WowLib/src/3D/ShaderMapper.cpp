/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */

#include "ShaderMapper.h"
#include "../log.h"

#include <array>
#include <string>
#include <string_view>
#include <optional>

namespace shader_mapper {

struct ShaderEntry {
	std::string_view PS;
	std::string_view VS;
	std::string_view HS;
	std::string_view DS;
};

static constexpr std::array<ShaderEntry, 36> SHADER_ARRAY = {{
	{"Combiners_Opaque_Mod2xNA_Alpha",           "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_AddAlpha",                "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_AddAlpha_Alpha",          "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Mod2xNA_Alpha_Add",       "Diffuse_T1_Env_T1",      "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Mod_AddAlpha",                   "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_AddAlpha",                "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_AddAlpha",                   "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_AddAlpha_Alpha",             "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Alpha_Alpha",             "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Mod2xNA_Alpha_3s",        "Diffuse_T1_Env_T1",      "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Opaque_AddAlpha_Wgt",            "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_Add_Alpha",                  "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_ModNA_Alpha",             "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_AddAlpha_Wgt",               "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_AddAlpha_Wgt",               "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_AddAlpha_Wgt",            "Diffuse_T1_T2",          "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Mod_Add_Wgt",             "Diffuse_T1_Env",         "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Mod2xNA_Alpha_UnshAlpha", "Diffuse_T1_Env_T1",      "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Mod_Dual_Crossfade",             "Diffuse_T1",             "T1",       "T1"       },
	{"Combiners_Mod_Depth",                      "Diffuse_EdgeFade_T1",    "T1",       "T1"       },
	{"Combiners_Opaque_Mod2xNA_Alpha_Alpha",     "Diffuse_T1_Env_T2",      "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Mod_Mod",                        "Diffuse_EdgeFade_T1_T2", "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_Masked_Dual_Crossfade",      "Diffuse_T1_T2",          "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Alpha",                   "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Opaque_Mod2xNA_Alpha_UnshAlpha", "Diffuse_T1_Env_T2",      "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Mod_Depth",                      "Diffuse_EdgeFade_Env",   "T1",       "T1"       },
	{"Guild",                                    "Diffuse_T1_T2_T1",       "T1_T2_T3", "T1_T2"    },
	{"Guild_NoBorder",                           "Diffuse_T1_T2",          "T1_T2",    "T1_T2_T3" },
	{"Guild_Opaque",                             "Diffuse_T1_T2_T1",       "T1_T2_T3", "T1_T2"    },
	{"Illum",                                    "Diffuse_T1_T1",          "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_Mod_Mod_Const",              "Diffuse_T1_T2_T3",       "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Mod_Mod_Mod_Const",              "Color_T1_T2_T3",         "T1_T2_T3", "T1_T2_T3" },
	{"Combiners_Opaque",                         "Diffuse_T1",             "T1",       "T1"       },
	{"Combiners_Mod_Mod2x",                      "Diffuse_EdgeFade_T1_T2", "T1_T2",    "T1_T2"    },
	{"Combiners_Mod",                            "Diffuse_EdgeFade_T1",    "T1_T2",    "T1_T2"    },
	{"Combiners_Mod_Mod_Depth",                  "Diffuse_EdgeFade_T1_T2", "T1_T2",    "T1_T2"    },
}};


/**
 * Gets Vertex shader name from shader ID
 */
std::optional<std::string> getVertexShader(int textureCount, int shaderID) {
	if (shaderID & 0x8000) {
		const int vertexShaderId = shaderID & 0x7FFF;
		if (vertexShaderId >= static_cast<int>(SHADER_ARRAY.size())) {
			logging::write("Unknown vertex shader ID: " + std::to_string(vertexShaderId));
			return std::nullopt;
		}

		return std::string(SHADER_ARRAY[vertexShaderId].VS);
	}
	else {
		if (textureCount == 1) {
			if (shaderID & 0x80) {
				return "Diffuse_Env";
			} else {
				if (shaderID & 0x4000)
					return "Diffuse_T2";
				else
					return "Diffuse_T1";
			}
		} else {
			if (shaderID & 0x80) {
				if (shaderID & 0x8)
					return "Diffuse_Env_Env";
				else
					return "Diffuse_Env_T1";
			} else {
				if (shaderID & 0x8) {
					return "Diffuse_T1_Env";
				} else {
					if (shaderID & 0x4000)
						return "Diffuse_T1_T2";
					else
						return "Diffuse_T1_T1";
				}
			}
		}
	}
	return std::nullopt;
}

/**
 * Gets Pixel shader name from shader ID
 */
std::optional<std::string> getPixelShader(int textureCount, int shaderID) {
	if (shaderID & 0x8000) {
		const int pixelShaderID = shaderID & 0x7FFF;
		if (pixelShaderID >= static_cast<int>(SHADER_ARRAY.size())) {
			logging::write("Unknown pixel shader ID: " + std::to_string(pixelShaderID));
			return std::nullopt;
		}

		return std::string(SHADER_ARRAY[pixelShaderID].PS);
	}
	else if (textureCount == 1)
	{
		return (shaderID & 0x70) ? "Combiners_Mod" : "Combiners_Opaque";
	}
	else
	{
		if (shaderID & 0x70) {
			switch (shaderID & 7) {
				case 3:
					return "Combiners_Mod_Add";
				case 4:
					return "Combiners_Mod_Mod2x";
				case 6:
					return "Combiners_Mod_Mod2xNA";
				case 7:
					return "Combiners_Mod_AddNA";
				default:
					return "Combiners_Mod_Mod";
			}
		}
		else {
			switch (shaderID & 7) {
				case 0:
					return "Combiners_Opaque_Opaque";
				case 3:
				case 7:
					return "Combiners_Opaque_AddAlpha";
				case 4:
					return "Combiners_Opaque_Mod2x";
				case 6:
					return "Combiners_Opaque_Mod2xNA";
				default:
					return "Combiners_Opaque_Mod";
			}
		}
	}
	return std::nullopt;
}

/**
 * Gets Hull shader name from shader ID
 */
std::optional<std::string> getHullShader(int textureCount, int shaderID) {
	if (shaderID & 0x8000) {
		const int hullShaderID = shaderID & 0x7FFF;
		if (hullShaderID >= static_cast<int>(SHADER_ARRAY.size())) {
			logging::write("Unknown hull shader ID: " + std::to_string(hullShaderID));
			return std::nullopt;
		}

		return std::string(SHADER_ARRAY[hullShaderID].HS);
	} else {
		if (textureCount == 1)
			return "T1";
		else
			return "T1_T2";
	}
	return std::nullopt;
}

/**
 * Gets Domain shader name from shader ID
 */
std::optional<std::string> getDomainShader(int textureCount, int shaderID) {
	if (shaderID & 0x8000) {
		const int domainShaderID = shaderID & 0x7FFF;
		if (domainShaderID >= static_cast<int>(SHADER_ARRAY.size())) {
			logging::write("Unknown domain shader ID: " + std::to_string(domainShaderID));
			return std::nullopt;
		}

		return std::string(SHADER_ARRAY[domainShaderID].DS);
	} else {
		if (textureCount == 1)
			return "T1";
		else
			return "T1_T2";
	}
	return std::nullopt;
}

}

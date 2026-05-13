/*!
	wow.export (https://github.com/Kruithne/wow.export)
	Authors: Kruithne <kruithne@gmail.com>, Marlamin <marlamin@marlamin.com>
	License: MIT
 */
#pragma once

#include <string>
#include <optional>

namespace shader_mapper {

/**
 * Gets Vertex shader name from shader ID
 */
std::optional<std::string> getVertexShader(int textureCount, int shaderID);

/**
 * Gets Pixel shader name from shader ID
 */
std::optional<std::string> getPixelShader(int textureCount, int shaderID);

/**
 * Gets Hull shader name from shader ID
 */
std::optional<std::string> getHullShader(int textureCount, int shaderID);

/**
 * Gets Domain shader name from shader ID
 */
std::optional<std::string> getDomainShader(int textureCount, int shaderID);

} // namespace shader_mapper

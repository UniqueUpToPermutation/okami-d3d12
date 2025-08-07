#pragma once

#include <filesystem>

namespace okami {
	std::filesystem::path GetExecutablePath();

	std::filesystem::path GetAssetsPath();
	std::filesystem::path GetAssetPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetShadersPath();
	std::filesystem::path GetShaderPath(const std::filesystem::path& relativePath);

	std::filesystem::path GetTestAssetsPath();
	std::filesystem::path GetTestAssetPath(const std::filesystem::path& relativePath);
}
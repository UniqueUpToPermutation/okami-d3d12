#include "assets.hpp"

using namespace okami;

std::optional<std::filesystem::path> g_assetsPath = std::nullopt;

static std::filesystem::path FindAssetsPath() {
    // Check current directory for "assets"
    std::filesystem::path currentDir = std::filesystem::current_path();
    std::filesystem::path assetsDir = currentDir / "assets";
    if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
        return assetsDir;
    }

    // Go two folders up and check for "assets"
    std::filesystem::path twoUpDir = currentDir;
    for (int i = 0; i < 2; ++i) {
        if (twoUpDir.has_parent_path()) {
            twoUpDir = twoUpDir.parent_path();
        }
    }
    assetsDir = twoUpDir / "assets";
    if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
        return assetsDir;
    }

    // If not found, return empty path
    return std::filesystem::path();
}

std::filesystem::path okami::GetAssetsPath() {
	if (!g_assetsPath.has_value()) {
		g_assetsPath = FindAssetsPath();
	}
	return *g_assetsPath;
}
#include "paths.hpp"
#include <optional>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <unistd.h>
#include <climits>
#elif __APPLE__
#include <mach-o/dyld.h>
#include <vector>
#endif

using namespace okami;

std::optional<std::filesystem::path> g_exePath = std::nullopt;
std::optional<std::filesystem::path> g_assetsPath = std::nullopt;
std::optional<std::filesystem::path> g_shadersPath = std::nullopt;
std::optional<std::filesystem::path> g_testAssetsPath = std::nullopt;

static std::filesystem::path SearchForPath(const std::filesystem::path& startPath, const std::string& targetDir) {
    // Check current directory for "assets"
    std::filesystem::path currentDir = GetExecutablePath().parent_path();
    std::filesystem::path assetsDir = currentDir / targetDir;
    if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
        return assetsDir;
    }

    // Go up the directory tree to find "assets"
    while (currentDir.has_parent_path()) {
        currentDir = currentDir.parent_path();
        assetsDir = currentDir / targetDir;
        if (std::filesystem::exists(assetsDir) && std::filesystem::is_directory(assetsDir)) {
            return assetsDir;
        }
    }

    // If not found, return empty path
    return std::filesystem::path();
}

static std::filesystem::path FindAssetsPath() {
	return SearchForPath(std::filesystem::current_path(), "assets");
}

static std::filesystem::path FindShadersPath() {
    return SearchForPath(std::filesystem::current_path(), "shaders");
}

static std::filesystem::path FindTestAssetsPath() {
    return SearchForPath(std::filesystem::current_path(), "tests/assets");
}

static std::filesystem::path FindExecutablePath() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer);
    }
#elif __linux__
    char buffer[PATH_MAX];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length != -1) {
        buffer[length] = '\0';
        return std::filesystem::path(buffer);
    }
#elif __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
        return std::filesystem::path(buffer.data());
    }
#endif
    return std::filesystem::path();
}

std::filesystem::path okami::GetAssetsPath() {
	if (!g_assetsPath.has_value()) {
		g_assetsPath = FindAssetsPath();
	}
	return *g_assetsPath;
}

std::filesystem::path okami::GetAssetPath(const std::filesystem::path& relativePath) {
    return GetAssetsPath() / relativePath;
}

std::filesystem::path okami::GetShadersPath() {
    if (!g_shadersPath.has_value()) {
        g_shadersPath = FindShadersPath();
    }
	return *g_shadersPath;
}
std::filesystem::path okami::GetShaderPath(const std::filesystem::path& relativePath) {
	return GetShadersPath() / relativePath;
}

std::filesystem::path okami::GetExecutablePath() {
    if (!g_exePath.has_value()) {
        g_exePath = FindExecutablePath();
    }
    return *g_exePath;
}

std::filesystem::path okami::GetTestAssetsPath() {
    if (!g_testAssetsPath.has_value()) {
        g_testAssetsPath = FindTestAssetsPath();
    }
    return *g_testAssetsPath;
}

std::filesystem::path okami::GetTestAssetPath(const std::filesystem::path& relativePath) {
    return GetTestAssetsPath() / relativePath;
}
#include "d3d12_common.hpp"

#include <filesystem>
#include <fstream>

#include <d3dcompiler.h>

using namespace okami;

std::expected<ComPtr<ID3DBlob>, Error> okami::LoadShaderFromFile(const std::string& filename) {
	ComPtr<ID3DBlob> shaderBlob;
    std::filesystem::path shaderPath = std::filesystem::current_path() / "shaders" / filename;

    // Try to read the compiled shader file
    std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::unexpected(Error("Failed to open shader file: " + shaderPath.string()));
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        return std::unexpected(Error("Failed to read shader file: " + shaderPath.string()));
    }

    HRESULT hr = D3DCreateBlob(size, &shaderBlob);
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create blob for shader: " + filename));
    }

    memcpy(shaderBlob->GetBufferPointer(), buffer.data(), size);
	return shaderBlob; // Success
}
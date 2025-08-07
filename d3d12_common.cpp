#include "d3d12_common.hpp"

#include <filesystem>
#include <fstream>

#include <d3dcompiler.h>

#include <glog/logging.h>

#include "paths.hpp"

using namespace okami;

std::expected<ComPtr<ID3DBlob>, Error> okami::LoadShaderFromFile(std::filesystem::path const& path) {
	ComPtr<ID3DBlob> shaderBlob;

    auto shaderPath = path.string();

    // Try to read the compiled shader file
    std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG(ERROR) << "Failed to open shader file: " << shaderPath;
        return std::unexpected(Error("Failed to open shader file: " + shaderPath));
    }

    std::streamsize m_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(m_size);
    if (!file.read(buffer.data(), m_size)) {
        return std::unexpected(Error("Failed to read shader file: " + shaderPath));
    }

    HRESULT hr = D3DCreateBlob(m_size, &shaderBlob);
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create blob for shader: " + shaderPath));
    }

    memcpy(shaderBlob->GetBufferPointer(), buffer.data(), m_size);
	return shaderBlob; // Success
}

hlsl::Camera okami::ToHLSLCamera(
    std::optional<Camera> camera, 
    std::optional<Transform> transform,
    int backbufferWidth,
    int backbufferHeight) {
    auto proj = camera.value_or(Camera::Identity()).GetProjectionMatrix(backbufferWidth, backbufferHeight, true);
    auto view = Inverse(transform.value_or(Transform::Identity())).AsMatrix();
    return hlsl::Camera{
        .m_viewMatrix = view,
        .m_projectionMatrix = proj,
        .m_viewProjectionMatrix = proj * view
	};
}

Expected<GPUBuffer> GPUBuffer::Create(
    ID3D12Device& device,
    size_t bufferSize) {
    ComPtr<ID3D12Resource> buffer;

    // Describe the new buffer resource
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create the buffer resource
    HRESULT hr = device.CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create structured buffer resource"));
    }

    return GPUBuffer(std::move(buffer));
}
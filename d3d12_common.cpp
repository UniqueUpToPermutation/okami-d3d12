#include "d3d12_common.hpp"

#include <filesystem>
#include <fstream>
#include <cstring>

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

Expected<GpuBuffer> GpuBuffer::Create(
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
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create structured buffer resource"));
    }

    return GpuBuffer(std::move(buffer));
}

std::vector<Attribute> okami::GetVertexAttributes(std::span<D3D12_INPUT_ELEMENT_DESC const> inputElements) {
    std::vector<Attribute> attributes;
    attributes.reserve(inputElements.size());

    for (const auto& element : inputElements) {
        Attribute attr = {};

        // Map semantic name to AttributeType
        if (strcmp(element.SemanticName, "POSITION") == 0) {
            attr.m_type = AttributeType::Position;
        } else if (strcmp(element.SemanticName, "NORMAL") == 0) {
            attr.m_type = AttributeType::Normal;
        } else if (strcmp(element.SemanticName, "TEXCOORD") == 0) {
            attr.m_type = AttributeType::TexCoord;
        } else if (strcmp(element.SemanticName, "COLOR") == 0) {
            attr.m_type = AttributeType::Color;
        } else if (strcmp(element.SemanticName, "TANGENT") == 0) {
            attr.m_type = AttributeType::Tangent;
        } else if (strcmp(element.SemanticName, "BITANGENT") == 0 || strcmp(element.SemanticName, "BINORMAL") == 0) {
            attr.m_type = AttributeType::Bitangent;
        } else {
            // Unknown semantic, skip this element
            LOG(WARNING) << "Unknown vertex semantic: " << element.SemanticName;
            continue;
        }

        // Set buffer index
        attr.m_bufferIndex = static_cast<int>(element.InputSlot);

        // Set offset
        attr.m_offset = static_cast<size_t>(element.AlignedByteOffset);

        // Calculate size based on DXGI format
        attr.m_size = GetFormatSize(element.Format);

        // For stride, we need to calculate it based on the input elements
        // Find the maximum offset + size for this input slot to determine stride
        size_t maxOffsetPlusSize = attr.m_offset + attr.m_size;
        for (const auto& otherElement : inputElements) {
            if (otherElement.InputSlot == element.InputSlot) {
                size_t otherOffsetPlusSize = otherElement.AlignedByteOffset + GetFormatSize(otherElement.Format);
                maxOffsetPlusSize = std::max(maxOffsetPlusSize, otherOffsetPlusSize);
            }
        }
        attr.m_stride = maxOffsetPlusSize;

        attributes.push_back(attr);
    }

    return attributes;
}

size_t okami::GetFormatSize(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R32_FLOAT:
            return 4;
        case DXGI_FORMAT_R32G32_FLOAT:
            return 8;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 16;
        case DXGI_FORMAT_R16G16_FLOAT:
            return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return 8;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
            return 4;
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R16G16_UNORM:
            return 4;
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_R16_UNORM:
            return 2;
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SINT:
            return 1;
        default:
            LOG(WARNING) << "Unknown DXGI format: " << static_cast<int>(format);
            return 0;
    }
}
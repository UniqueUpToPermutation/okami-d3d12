#ifdef USE_D3D12

#include "d3d12_texture.hpp"

#include <glog/logging.h>
#include <directx/d3dx12.h>

using namespace okami;

DXGI_FORMAT TextureLoadTask::TextureFormatToDXGI(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
            return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8:
            return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:
            return DXGI_FORMAT_R8G8B8A8_UNORM; // D3D12 doesn't support RGB8, use RGBA8
        case TextureFormat::RGBA8:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::R32F:
            return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::RG32F:
            return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::RGB32F:
            return DXGI_FORMAT_R32G32B32_FLOAT;
        case TextureFormat::RGBA32F:
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}

uint32_t TextureLoadTask::GetBytesPerPixel(TextureFormat format) {
    return GetPixelStride(format);
}

Error TextureLoadTask::Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) {
    std::optional<RawTexture> rawTexture;

    if (m_path) {
        // Load from file
        auto result = RawTexture::FromPNG(*m_path);
        if (!result.has_value()) {
            return result.error();
        }
        rawTexture = std::move(result.value());
    } else if (m_initData) {
        // Use provided data
        rawTexture = std::move(*m_initData);
    } else {
        return Error("No texture data provided");
    }

    const auto& info = rawTexture->GetInfo();
    auto textureData = rawTexture->GetData();

    // Convert texture format to DXGI format
    DXGI_FORMAT dxgiFormat = TextureFormatToDXGI(info.format);
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
        return Error("Unsupported texture format");
    }

    // Create texture resource
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = info.width;
    textureDesc.Height = info.height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = info.mipLevels;
    textureDesc.Format = dxgiFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    TexturePrivate privateData;

    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = device.CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&privateData.m_resource));

    if (FAILED(hr)) {
        return Error("Failed to create texture resource");
    }
    privateData.m_resource->SetName(L"Okami Managed Texture Resource");

    // Store texture properties
    privateData.m_dxgiFormat = dxgiFormat;

    // Create upload buffer
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(privateData.m_resource.Get(), 0, 1);

    heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    hr = device.CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer));

    if (FAILED(hr)) {
        return Error("Failed to create texture upload buffer");
    }
    m_uploadBuffer->SetName(L"Okami Managed Texture Upload Buffer");

    // Prepare subresource data
    D3D12_SUBRESOURCE_DATA subresourceData = {};
    subresourceData.pData = textureData.data();
    subresourceData.RowPitch = info.width * GetBytesPerPixel(info.format);
    subresourceData.SlicePitch = subresourceData.RowPitch * info.height;

    // Upload texture data
    UpdateSubresources(&commandList, privateData.m_resource.Get(), m_uploadBuffer.Get(), 0, 0, 1, &subresourceData);

    // Set resource data
    m_resource.m_info = info;
    m_resource.m_privateData = std::move(privateData);

    return {};
}

Error TextureLoadTask::Finalize() {
    return m_manager->Finalize(m_resourceId, std::move(m_resource), GetError());
}

Expected<std::shared_ptr<TextureManager>> TextureManager::Create(
    ID3D12Device& device,
    std::shared_ptr<GpuUploader> uploader) {
    std::shared_ptr<TextureManager> manager = std::shared_ptr<TextureManager>(new TextureManager(uploader));

    auto pool = DescriptorPool::Create(
        &device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        kMinPoolSize,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    
    if (!pool.has_value()) {
        return std::unexpected(pool.error());
    }

    manager->m_srvDescriptorPool = std::move(pool.value());
    manager->m_sizer.m_minSize = kMinPoolSize;
    manager->m_sizer.Reset(kMinPoolSize);

    return manager;
}

Error TextureManager::TransitionTextures(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) {
    // Transition all textures that need to be transitioned
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    while (!m_texturesToTransition.empty()) {
        auto nextResourceId = m_texturesToTransition.front();
        m_texturesToTransition.pop();

        auto it = m_texturesById.find(nextResourceId);
        if (it == m_texturesById.end()) {
            LOG(WARNING) << "Texture with ID " << nextResourceId << " not found for transition";
            continue;
        }
        auto& texture = it->second;

        auto& privateData = std::any_cast<TexturePrivate&>(texture->m_data.m_privateData);

        // Texture transition
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = privateData.m_resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers.push_back(barrier);

        // Create shader resource view
        auto handle = m_srvDescriptorPool.TryAlloc();

        if (!handle) {
            continue;
        }

        auto srvDesc = privateData.GetSRVDesc(texture->m_data.m_info);
        device.CreateShaderResourceView(
            privateData.m_resource.Get(),
            &srvDesc,
            m_srvDescriptorPool.GetCpuHandle(*handle));
        privateData.m_handle = *handle;
    }

    if (!barriers.empty()) {
        commandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }

    // Regenerate all shader resource views
    auto newSize = m_sizer.GetNextSize(m_texturesById.size());
    if (newSize) {
        return RegenerateSRVs(device, static_cast<uint32_t>(*newSize));
    }

    return {};
}

Error TextureManager::RegenerateSRVs(ID3D12Device& device, uint32_t poolSize) {
    m_srvDescriptorPool = {};

    auto newPool = DescriptorPool::Create(
        &device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        poolSize,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    if (!newPool.has_value()) {
        LOG(WARNING) << "Failed to create new descriptor pool";
        return newPool.error();
    }

    m_srvDescriptorPool = std::move(newPool.value());

    for (auto it = m_texturesById.begin(); it != m_texturesById.end(); ++it) {
        auto& texture = it->second;

        // Create shader resource view
        auto handle = m_srvDescriptorPool.TryAlloc();
        if (!handle) {
            LOG(WARNING) << "Failed to allocate descriptor handle for texture " << it->first;
            continue;
        }

        auto& privateData = std::any_cast<TexturePrivate&>(texture->m_data.m_privateData);

        // Create SRV for the texture
        auto srvDesc = privateData.GetSRVDesc(texture->m_data.m_info);
        device.CreateShaderResourceView(
            privateData.m_resource.Get(),
            &srvDesc,
            m_srvDescriptorPool.GetCpuHandle(*handle));
        privateData.m_handle = *handle;
    }

    return {};
}

D3D12_SHADER_RESOURCE_VIEW_DESC TexturePrivate::GetSRVDesc(TextureInfo const& info) const {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = m_dxgiFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = info.mipLevels;
    return srvDesc;
}

std::pair<resource_id_t, ResHandle<Texture>> TextureManager::NewResource(
    std::optional<std::string_view> path) {
    auto newId = m_nextResourceId.fetch_add(1);
    auto texture = std::make_unique<Resource<Texture>>();
    texture->m_id = newId;
    texture->m_path = std::string(path.value_or(""));
    auto handle = ResHandle<Texture>(texture.get());
    m_texturesById.emplace(newId, std::move(texture));
    return { newId, handle };
}

Error TextureManager::Finalize(resource_id_t resourceId,
    Texture data, 
    Error error) {
    auto it = m_texturesById.find(resourceId);
    if (it == m_texturesById.end()) {
        return Error("Texture not found");
    }

    // Update texture info
    it->second->m_data = std::move(data);

    if (!error.IsError()) {
        m_texturesToTransition.push(resourceId);
    }

    it->second->m_loaded.store(true);

    return {};
}

ResHandle<Texture> TextureManager::Load(std::string_view path) {
    // Find existing texture if already loaded
    if (auto it = m_texturePathsToIds.find(path); it != m_texturePathsToIds.end()) {
        auto resourceId = it->second;
        auto textureIt = m_texturesById.find(resourceId);
        if (textureIt != m_texturesById.end()) {
            return textureIt->second.get();
        }
    }

    auto [resourceId, handle] = NewResource(path);
    m_uploader->SubmitTask(
        std::make_unique<TextureLoadTask>(
            std::filesystem::path(path),
            resourceId,
            this));
    m_texturePathsToIds.emplace(path, resourceId);
    return handle;
}

ResHandle<Texture> TextureManager::Create(typename Texture::CreationData&& data) {
    auto [resourceId, handle] = NewResource();
    m_uploader->SubmitTask(
        std::make_unique<TextureLoadTask>(
            std::move(data),
            resourceId,
            this));
    return handle;
}

#endif
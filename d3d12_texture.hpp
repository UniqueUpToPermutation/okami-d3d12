#pragma once

#include "renderer.hpp"
#include "d3d12_common.hpp"
#include "d3d12_upload.hpp"
#include "storage.hpp"
#include "texture.hpp"
#include "d3d12_descriptor_pool.hpp"

namespace okami {
    class TextureManager;

    struct TexturePrivate {
        ComPtr<ID3D12Resource> m_resource;
        DescriptorPool::Handle m_handle;
        DXGI_FORMAT m_dxgiFormat = DXGI_FORMAT_UNKNOWN;
    };

    struct TextureImpl {
        std::unique_ptr<Resource<Texture>> m_public;
        TexturePrivate m_private;

        D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc() const;
    };

    struct TextureLoadTask final : public GpuUploaderTask {
    private:
        std::optional<std::filesystem::path> m_path;
        std::optional<RawTexture> m_initData;
        resource_id_t m_resourceId;
        TextureManager* m_manager = nullptr;
        TexturePrivate m_privateData;
        Texture m_publicData;

        // Temporary upload buffers
        // These will be released after the GPU upload is complete
        ComPtr<ID3D12Resource> m_uploadBuffer;

    public:
        inline TextureLoadTask(
            std::filesystem::path path,
            resource_id_t resourceId,
            TextureManager* manager) :
            m_path(std::move(path)), m_resourceId(resourceId), m_manager(manager) {}

        inline TextureLoadTask(
            RawTexture initData,
            resource_id_t resourceId,
            TextureManager* manager) :
            m_initData(std::move(initData)), m_resourceId(resourceId), m_manager(manager) {}

        Error Execute(ID3D12Device& device, ID3D12GraphicsCommandList& commandList) override;
        Error Finalize() override;

    private:
        DXGI_FORMAT TextureFormatToDXGI(TextureFormat format);
        uint32_t GetBytesPerPixel(TextureFormat format);
    };

    class TextureManager : public IResourceManager<Texture> {
    private:
        std::unordered_map<std::filesystem::path, resource_id_t> m_texturePathsToIds;
        std::unordered_map<resource_id_t, TextureImpl> m_texturesById;

        std::queue<resource_id_t> m_texturesToTransition;
        std::atomic<resource_id_t> m_nextResourceId{0};

        std::shared_ptr<GpuUploader> m_uploader;
       
        DescriptorPool m_srvDescriptorPool;
        Sizer m_sizer;
    
        static constexpr size_t kMinPoolSize = 128;

        inline TextureManager(std::shared_ptr<GpuUploader> uploader) : m_uploader(std::move(uploader)) {}

    public:
        OKAMI_NO_COPY(TextureManager);
        OKAMI_NO_MOVE(TextureManager);

        static Expected<std::shared_ptr<TextureManager>> Create(
            ID3D12Device& device,
            std::shared_ptr<GpuUploader> uploader
        );

        inline void Register(InterfaceCollection& queryable) {
            queryable.Register<IResourceManager<Texture>>(this);
        }

        inline std::unordered_map<resource_id_t, TextureImpl> const& GetTextures() const {
            return m_texturesById;
        }

        inline ID3D12DescriptorHeap* GetSrvHeap() const {
            return m_srvDescriptorPool.GetHeap();
        }

        inline DescriptorPool const& GetSrvPool() {
            return m_srvDescriptorPool;
        }

        std::pair<resource_id_t, ResHandle<Texture>> NewResource(
            std::optional<std::string_view> path = std::nullopt);

        Error Finalize(
            resource_id_t resourceId,
            Texture publicData, 
            TexturePrivate privateData,
            Error error);

        ResHandle<Texture> Load(std::string_view path) override;
        ResHandle<Texture> Create(typename Texture::CreationData&& data) override;

        Error RegenerateSRVs(ID3D12Device& device, uint32_t poolSize);
        Error TransitionTextures(ID3D12Device& device, ID3D12GraphicsCommandList& commandList);
    };
}
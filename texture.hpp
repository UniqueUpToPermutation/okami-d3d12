#pragma once

#include <vector>
#include <span>
#include <filesystem>

#include "common.hpp"

namespace okami {
    enum class TextureType {
        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_2D_ARRAY,
        TEXTURE_3D,
        TEXTURE_CUBE
    };

    enum class TextureFormat {
        R8,
        RG8,
        RGB8,
        RGBA8,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F,
    };

    struct TextureInfo {
        TextureType type;
        TextureFormat format;
        uint32_t width;
        uint32_t height;
        uint32_t depth;
        uint32_t arraySize;
        uint32_t mipLevels;
    };

    uint32_t GetChannelCount(TextureFormat format);
    uint32_t GetPixelStride(TextureFormat format);
    uint32_t GetTextureSize(TextureInfo const& info);

    class RawTexture {
    private:
        TextureInfo m_info;
        std::vector<uint8_t> m_data; // Raw texture data
    public:

        RawTexture(const TextureInfo& info)
            : m_info(info), m_data(GetTextureSize(info), 0) {}

        inline const TextureInfo& GetInfo() const { 
            return m_info; 
        }
        inline const std::span<uint8_t const> GetData() const {
            return std::span<uint8_t const>(m_data.data(), m_data.size());
        }

        static Expected<RawTexture> FromPNG(const std::filesystem::path& path);
    };

    class Texture {
    public:
        TextureInfo m_info;

        inline uint32_t GetWidth() const { return m_info.width; }
        inline uint32_t GetHeight() const { return m_info.height; }
        inline uint32_t GetDepth() const { return m_info.depth; }
        inline uint32_t GetArraySize() const { return m_info.arraySize; }
        inline uint32_t GetMipLevels() const { return m_info.mipLevels; }

        using CreationData = RawTexture;
    };
}
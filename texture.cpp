#include "texture.hpp"
#include "lodepng.h"
#include <cmath>
#include <algorithm>

namespace okami {

uint32_t GetChannelCount(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
        case TextureFormat::R32F:
            return 1;
        case TextureFormat::RG8:
        case TextureFormat::RG32F:
            return 2;
        case TextureFormat::RGB8:
        case TextureFormat::RGB32F:
            return 3;
        case TextureFormat::RGBA8:
        case TextureFormat::RGBA32F:
            return 4;
        default:
            return 0;
    }
}

uint32_t GetPixelStride(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8:
            return 1;
        case TextureFormat::RG8:
            return 2;
        case TextureFormat::RGB8:
            return 3;
        case TextureFormat::RGBA8:
            return 4;
        case TextureFormat::R32F:
            return 4;
        case TextureFormat::RG32F:
            return 8;
        case TextureFormat::RGB32F:
            return 12;
        case TextureFormat::RGBA32F:
            return 16;
        default:
            return 0;
    }
}

uint32_t GetTextureSize(const TextureInfo& info) {
    uint32_t pixelStride = GetPixelStride(info.format);
    uint32_t totalSize = 0;
    
    // Calculate size for all mip levels
    for (uint32_t mip = 0; mip < info.mipLevels; ++mip) {
        uint32_t mipWidth = std::max(1u, info.width >> mip);
        uint32_t mipHeight = std::max(1u, info.height >> mip);
        uint32_t mipDepth = std::max(1u, info.depth >> mip);
        
        uint32_t mipSize = mipWidth * mipHeight * mipDepth * pixelStride;
        
        // For texture arrays, multiply by array size
        if (info.type == TextureType::TEXTURE_2D_ARRAY) {
            mipSize *= info.arraySize;
        }
        
        totalSize += mipSize;
    }
    
    return totalSize;
}

Expected<RawTexture> RawTexture::FromPNG(const std::filesystem::path& path) {
    // Check if file exists
    if (!std::filesystem::exists(path)) {
        return std::unexpected(Error("PNG file does not exist: " + path.string()));
    }
    
    // Convert path to string for lodepng (which expects char*)
    std::string pathStr = path.string();
    
    unsigned char* imageData = nullptr;
    unsigned width, height;
    
    // Load PNG with 32-bit RGBA format
    unsigned error = lodepng_decode32_file(&imageData, &width, &height, pathStr.c_str());
    
    if (error) {
        return std::unexpected(Error("Failed to load PNG: " + std::string(lodepng_error_text(error))));
    }
    
    // Ensure we have valid data
    if (!imageData || width == 0 || height == 0) {
        if (imageData) {
            free(imageData);
        }
        return std::unexpected(Error("Invalid PNG data"));
    }
    
    // Create texture info - PNG loaded as RGBA8, 2D texture, single mip level
    TextureInfo info = {};
    info.type = TextureType::TEXTURE_2D;
    info.format = TextureFormat::RGBA8;
    info.width = width;
    info.height = height;
    info.depth = 1;
    info.arraySize = 1;
    info.mipLevels = 1;
    
    // Create texture
    RawTexture texture(info);
    
    // Copy data to texture
    uint32_t dataSize = width * height * 4; // RGBA8 = 4 bytes per pixel
    std::copy(imageData, imageData + dataSize, texture.m_data.begin());
    
    // Free lodepng allocated memory
    free(imageData);
    
    return texture;
}

} // namespace okami

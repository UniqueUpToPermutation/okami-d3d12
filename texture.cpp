#include <cmath>
#include <algorithm>
#include <fstream>

#include <glog/logging.h>

#include "texture.hpp"
#include "lodepng.h"

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

Error RawTexture::SavePNG(const std::filesystem::path& path) const {
    // PNG only supports certain formats, so we need to convert
    std::vector<uint8_t> pngData;
    unsigned width = m_info.width;
    unsigned height = m_info.height;
    LodePNGColorType colorType;
    unsigned bitDepth = 8;

    // Convert texture data to PNG-compatible format
    switch (m_info.format) {
        case TextureFormat::R8: {
            colorType = LCT_GREY;
            pngData = m_data; // Direct copy for single channel 8-bit
            break;
        }
        case TextureFormat::RG8: {
            colorType = LCT_GREY_ALPHA;
            pngData = m_data; // Direct copy for two channel 8-bit
            break;
        }
        case TextureFormat::RGB8: {
            colorType = LCT_RGB;
            pngData = m_data; // Direct copy for three channel 8-bit
            break;
        }
        case TextureFormat::RGBA8: {
            colorType = LCT_RGBA;
            pngData = m_data; // Direct copy for four channel 8-bit
            break;
        }
        case TextureFormat::R32F: {
            // Convert R32F to R8
            colorType = LCT_GREY;
            pngData.resize(width * height);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RG32F: {
            // Convert RG32F to RG8
            colorType = LCT_GREY_ALPHA;
            pngData.resize(width * height * 2);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 2; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RGB32F: {
            // Convert RGB32F to RGB8
            colorType = LCT_RGB;
            pngData.resize(width * height * 3);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 3; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        case TextureFormat::RGBA32F: {
            // Convert RGBA32F to RGBA8
            colorType = LCT_RGBA;
            pngData.resize(width * height * 4);
            const float* srcData = reinterpret_cast<const float*>(m_data.data());
            for (size_t i = 0; i < width * height * 4; ++i) {
                // Clamp to [0,1] and convert to 8-bit
                float val = std::clamp(srcData[i], 0.0f, 1.0f);
                pngData[i] = static_cast<uint8_t>(val * 255.0f);
            }
            break;
        }
        default:
            return Error("Unsupported texture format for PNG export: " + std::to_string(static_cast<int>(m_info.format)));
    }

    // Only support 2D textures for PNG export
    if (m_info.type != TextureType::TEXTURE_2D) {
        return Error("PNG export only supports 2D textures");
    }

    // Only export the first mip level
    if (m_info.mipLevels > 1) {
        LOG(WARNING) << "PNG export will only save the first mip level of texture";
    }

    // Only export the first array slice
    if (m_info.arraySize > 1) {
        LOG(WARNING) << "PNG export will only save the first array slice of texture";
    }

    // Use lodepng to encode and save the PNG
    std::vector<uint8_t> encodedPng;
    unsigned error = lodepng::encode(encodedPng, pngData, width, height, colorType, bitDepth);
    
    if (error) {
        return Error("LodePNG encoding error: " + std::string(lodepng_error_text(error)));
    }

    // Save to file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Error("Failed to open file for writing: " + path.string());
    }

    file.write(reinterpret_cast<const char*>(encodedPng.data()), encodedPng.size());
    file.close();

    if (!file.good()) {
        return Error("Failed to write PNG data to file: " + path.string());
    }

    return {};
}

} // namespace okami

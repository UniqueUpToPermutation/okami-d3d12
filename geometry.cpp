#include "geometry.hpp"
#include <tiny_gltf.h>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <glog/logging.h>

using namespace okami;

Expected<Geometry> Geometry::LoadGLTF(std::filesystem::path const& path, std::span<Attribute const> attributes) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;
    
    // Check file extension to determine if it's binary or text
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    if (extension == ".glb") {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
    } else if (extension == ".gltf") {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
    } else {
        return std::unexpected(Error("Unsupported file format. Expected .gltf or .glb"));
    }

    if (!warn.empty()) {
        LOG(WARNING) << "TinyGLTF Warning: " << warn;
    }

    if (!err.empty()) {
        LOG(ERROR) << "TinyGLTF Error: " << err;
        return std::unexpected(Error("TinyGLTF failed to load file: " + err));
    }

    if (!ret) {
        return std::unexpected(Error("Failed to parse glTF file"));
    }

    // For now, we'll load the first mesh's first primitive
    if (model.meshes.empty()) {
        return std::unexpected(Error("No meshes found in glTF file"));
    }

    const auto& mesh = model.meshes[0];
    if (mesh.primitives.empty()) {
        return std::unexpected(Error("No primitives found in first mesh"));
    }

    const auto& primitive = mesh.primitives[0];

    // Map glTF attribute names to our AttributeType
    auto mapAttributeType = [](const std::string& name) -> std::optional<AttributeType> {
        if (name == "POSITION") return AttributeType::Position;
        if (name == "NORMAL") return AttributeType::Normal;
        if (name == "TEXCOORD_0") return AttributeType::TexCoord;
        if (name == "COLOR_0") return AttributeType::Color;
        if (name == "TANGENT") return AttributeType::Tangent;
        if (name == "BITANGENT") return AttributeType::Bitangent;
        return std::nullopt;
    };

    // Helper function to get component size
    auto getComponentSize = [](int componentType) -> size_t {
        switch (componentType) {
            case TINYGLTF_COMPONENT_TYPE_BYTE:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return 1;
            case TINYGLTF_COMPONENT_TYPE_SHORT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return 2;
            case TINYGLTF_COMPONENT_TYPE_INT:
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return 4;
            case TINYGLTF_COMPONENT_TYPE_DOUBLE:
                return 8;
            default:
                return 0;
        }
    };

    // Helper function to get number of components
    auto getComponentCount = [](int type) -> size_t {
        switch (type) {
            case TINYGLTF_TYPE_SCALAR: return 1;
            case TINYGLTF_TYPE_VEC2: return 2;
            case TINYGLTF_TYPE_VEC3: return 3;
            case TINYGLTF_TYPE_VEC4: return 4;
            case TINYGLTF_TYPE_MAT2: return 4;
            case TINYGLTF_TYPE_MAT3: return 9;
            case TINYGLTF_TYPE_MAT4: return 16;
            default: return 0;
        }
    };

    // Determine the maximum buffer index to know how many buffers we need
    size_t maxBufferIndex = 0;
    for (const auto& attr : attributes) {
        maxBufferIndex = std::max(maxBufferIndex, static_cast<size_t>(attr.m_bufferIndex));
    }
    
    // Create vertex buffers
    std::vector<std::vector<uint8_t>> vertexBuffers(maxBufferIndex + 1);
    
    // Find vertex count from any available attribute
    size_t vertexCount = 0;
    for (const auto& [attrName, accessorIndex] : primitive.attributes) {
        auto attrType = mapAttributeType(attrName);
        if (attrType) {
            const auto& accessor = model.accessors[accessorIndex];
            vertexCount = accessor.count;
            break;
        }
    }
    
    if (vertexCount == 0) {
        return std::unexpected(Error("No supported attributes found"));
    }

    // Calculate buffer sizes and initialize them
    for (size_t bufferIdx = 0; bufferIdx <= maxBufferIndex; ++bufferIdx) {
        size_t bufferStride = 0;
        for (const auto& attr : attributes) {
            if (attr.m_bufferIndex == bufferIdx) {
                bufferStride = std::max(bufferStride, attr.m_offset + attr.m_size);
            }
        }
        if (bufferStride > 0) {
            vertexBuffers[bufferIdx].resize(vertexCount * bufferStride);
        }
    }

    // Process each requested attribute
    std::vector<Attribute> resultAttributes;
    
    // Handle index buffer if available
    std::optional<std::vector<index_t>> indexBuffer;
    if (primitive.indices >= 0) {
        const auto& indexAccessor = model.accessors[primitive.indices];
        const auto& indexBufferView = model.bufferViews[indexAccessor.bufferView];
        const auto& indexBufferData = model.buffers[indexBufferView.buffer];
        
        const uint8_t* indexSrc = indexBufferData.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
        size_t indexCount = indexAccessor.count;
        
        indexBuffer = std::vector<index_t>(indexCount);
        
        // Convert different index types to index_t
        switch (indexAccessor.componentType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const uint8_t* indices = reinterpret_cast<const uint8_t*>(indexSrc);
                for (size_t i = 0; i < indexCount; ++i) {
                    (*indexBuffer)[i] = static_cast<index_t>(indices[i]);
                }
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const uint16_t* indices = reinterpret_cast<const uint16_t*>(indexSrc);
                for (size_t i = 0; i < indexCount; ++i) {
                    (*indexBuffer)[i] = static_cast<index_t>(indices[i]);
                }
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                const uint32_t* indices = reinterpret_cast<const uint32_t*>(indexSrc);
                for (size_t i = 0; i < indexCount; ++i) {
                    (*indexBuffer)[i] = indices[i];
                }
                break;
            }
            default:
                LOG(WARNING) << "Unsupported index component type: " << indexAccessor.componentType;
                indexBuffer.reset();
                break;
        }
    }
    
    for (const auto& requestedAttr : attributes) {
        // Find corresponding glTF attribute
        std::string gltfAttrName;
        switch (requestedAttr.m_type) {
            case AttributeType::Position: gltfAttrName = "POSITION"; break;
            case AttributeType::Normal: gltfAttrName = "NORMAL"; break;
            case AttributeType::TexCoord: gltfAttrName = "TEXCOORD_0"; break;
            case AttributeType::Color: gltfAttrName = "COLOR_0"; break;
            case AttributeType::Tangent: gltfAttrName = "TANGENT"; break;
            case AttributeType::Bitangent: gltfAttrName = "BITANGENT"; break;
            default:
                LOG(WARNING) << "Unsupported attribute type requested";
                continue;
        }

        auto attrIt = primitive.attributes.find(gltfAttrName);
        if (attrIt == primitive.attributes.end()) {
            LOG(WARNING) << "Requested attribute " << gltfAttrName << " not found in glTF file";
            continue;
        }

        const auto& accessor = model.accessors[attrIt->second];
        const auto& bufferView = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[bufferView.buffer];

        size_t componentSize = getComponentSize(accessor.componentType);
        size_t componentCount = getComponentCount(accessor.type);
        size_t actualSize = componentSize * componentCount;

        if (componentSize == 0 || componentCount == 0) {
            LOG(WARNING) << "Invalid component type/count for attribute: " << gltfAttrName;
            continue;
        }

        if (actualSize != requestedAttr.m_size) {
            LOG(WARNING) << "Size mismatch for attribute " << gltfAttrName 
                        << ": expected " << requestedAttr.m_size 
                        << ", got " << actualSize;
            continue;
        }

        if (accessor.count != vertexCount) {
            return std::unexpected(Error("Mismatched vertex counts between attributes"));
        }

        // Copy data to the appropriate buffer
        const uint8_t* src = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
        size_t srcStride = bufferView.byteStride ? bufferView.byteStride : actualSize;

        auto& targetBuffer = vertexBuffers[requestedAttr.m_bufferIndex];
        for (size_t vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx) {
            uint8_t* dst = targetBuffer.data() + vertexIdx * requestedAttr.m_stride + requestedAttr.m_offset;
            const uint8_t* srcVertex = src + vertexIdx * srcStride;
            std::memcpy(dst, srcVertex, actualSize);
        }

        // Add the attribute to our result
        resultAttributes.push_back(requestedAttr);
    }

    if (resultAttributes.empty()) {
        return std::unexpected(Error("No requested attributes could be loaded"));
    }

    return Geometry(std::move(resultAttributes), std::move(vertexBuffers), std::move(indexBuffer));
}

Expected<Geometry> Geometry::FromBuffers(GeometryBuffers const& buffers, std::span<Attribute const> attributes) {
    // Determine vertex count from the first available buffer
    size_t vertexCount = 0;
    
    // Check each buffer type to find vertex count
    if (buffers.positions.has_value()) {
        vertexCount = buffers.positions->size() / 3; // 3 floats per position (x, y, z)
    } else if (buffers.normals.has_value()) {
        vertexCount = buffers.normals->size() / 3; // 3 floats per normal
    } else if (buffers.texCoords.has_value()) {
        vertexCount = buffers.texCoords->size() / 2; // 2 floats per texture coordinate (u, v)
    } else if (buffers.tangents.has_value()) {
        vertexCount = buffers.tangents->size() / 3; // 3 floats per tangent (x, y, z)
    } else if (buffers.bitangents.has_value()) {
        vertexCount = buffers.bitangents->size() / 3; // 3 floats per bitangent
    } else {
        return std::unexpected(Error("No vertex data provided in buffers"));
    }
    
    if (vertexCount == 0) {
        return std::unexpected(Error("Empty vertex buffers provided"));
    }
    
    // Validate that all provided buffers have consistent vertex counts
    if (buffers.positions.has_value() && buffers.positions->size() / 3 != vertexCount) {
        return std::unexpected(Error("Position buffer vertex count mismatch"));
    }
    if (buffers.normals.has_value() && buffers.normals->size() / 3 != vertexCount) {
        return std::unexpected(Error("Normal buffer vertex count mismatch"));
    }
    if (buffers.texCoords.has_value() && buffers.texCoords->size() / 2 != vertexCount) {
        return std::unexpected(Error("Texture coordinate buffer vertex count mismatch"));
    }
    if (buffers.tangents.has_value() && buffers.tangents->size() / 3 != vertexCount) {
        return std::unexpected(Error("Tangent buffer vertex count mismatch"));
    }
    if (buffers.bitangents.has_value() && buffers.bitangents->size() / 3 != vertexCount) {
        return std::unexpected(Error("Bitangent buffer vertex count mismatch"));
    }
    
    // Determine the maximum buffer index to know how many buffers we need
    size_t maxBufferIndex = 0;
    for (const auto& attr : attributes) {
        maxBufferIndex = std::max(maxBufferIndex, static_cast<size_t>(attr.m_bufferIndex));
    }
    
    // Create vertex buffers
    std::vector<std::vector<uint8_t>> vertexBuffers(maxBufferIndex + 1);
    
    // Calculate buffer sizes and initialize them
    for (size_t bufferIdx = 0; bufferIdx <= maxBufferIndex; ++bufferIdx) {
        size_t bufferStride = 0;
        for (const auto& attr : attributes) {
            if (attr.m_bufferIndex == bufferIdx) {
                bufferStride = std::max(bufferStride, attr.m_offset + attr.m_size);
            }
        }
        if (bufferStride > 0) {
            vertexBuffers[bufferIdx].resize(vertexCount * bufferStride);
        }
    }
    
    // Process each requested attribute
    std::vector<Attribute> resultAttributes;
    for (const auto& requestedAttr : attributes) {
        const float* sourceData = nullptr;
        size_t sourceComponentCount = 0;
        
        // Get source data based on attribute type
        switch (requestedAttr.m_type) {
            case AttributeType::Position:
                if (buffers.positions.has_value()) {
                    sourceData = buffers.positions->data();
                    sourceComponentCount = 3;
                }
                break;
            case AttributeType::Normal:
                if (buffers.normals.has_value()) {
                    sourceData = buffers.normals->data();
                    sourceComponentCount = 3;
                }
                break;
            case AttributeType::TexCoord:
                if (buffers.texCoords.has_value()) {
                    sourceData = buffers.texCoords->data();
                    sourceComponentCount = 2;
                }
                break;
            case AttributeType::Tangent:
                if (buffers.tangents.has_value()) {
                    sourceData = buffers.tangents->data();
                    sourceComponentCount = 3;
                }
                break;
            case AttributeType::Bitangent:
                if (buffers.bitangents.has_value()) {
                    sourceData = buffers.bitangents->data();
                    sourceComponentCount = 3;
                }
                break;
            default:
                LOG(WARNING) << "Unsupported attribute type requested: " << static_cast<int>(requestedAttr.m_type);
                continue;
        }
        
        if (!sourceData) {
            LOG(WARNING) << "Requested attribute " << static_cast<int>(requestedAttr.m_type) << " not available in buffers";
            continue;
        }
        
        // Validate attribute size matches expected size
        size_t expectedSize = sourceComponentCount * sizeof(float);
        if (requestedAttr.m_size != expectedSize) {
            LOG(WARNING) << "Size mismatch for attribute " << static_cast<int>(requestedAttr.m_type)
                        << ": expected " << requestedAttr.m_size 
                        << ", got " << expectedSize;
            continue;
        }
        
        // Copy data to the appropriate buffer
        auto& targetBuffer = vertexBuffers[requestedAttr.m_bufferIndex];
        for (size_t vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx) {
            uint8_t* dst = targetBuffer.data() + vertexIdx * requestedAttr.m_stride + requestedAttr.m_offset;
            const uint8_t* src = reinterpret_cast<const uint8_t*>(sourceData + vertexIdx * sourceComponentCount);
            std::memcpy(dst, src, expectedSize);
        }
        
        // Add the attribute to our result
        resultAttributes.push_back(requestedAttr);
    }
    
    if (resultAttributes.empty()) {
        return std::unexpected(Error("No requested attributes could be loaded from buffers"));
    }
    
    // Handle index buffer if available
    std::optional<std::vector<index_t>> indexBuffer;
    if (buffers.indices.has_value()) {
        indexBuffer = std::vector<index_t>(buffers.indices->begin(), buffers.indices->end());
    }
    
    return Geometry(std::move(resultAttributes), std::move(vertexBuffers), std::move(indexBuffer));
}

Geometry Geometry::AsFormat(std::span<Attribute const> attributes) const {
    if (attributes.empty()) {
        return Geometry(); // Return empty geometry
    }

    // Calculate vertex count from existing geometry
    size_t vertexCount = 0;
    if (!m_attributes.empty()) {
        // Use the first attribute to determine vertex count
        const auto& firstAttr = m_attributes[0];
        size_t bufferSize = m_vertexBuffers[firstAttr.m_bufferIndex].size();
        vertexCount = bufferSize / firstAttr.m_stride;
    }

    if (vertexCount == 0) {
        return Geometry(); // No vertices to convert
    }

    // Determine how many buffers we need for the target format
    int maxBufferIndex = 0;
    for (const auto& attr : attributes) {
        maxBufferIndex = std::max(maxBufferIndex, attr.m_bufferIndex);
    }

    // Create new vertex buffers
    std::vector<std::vector<uint8_t>> newVertexBuffers(maxBufferIndex + 1);
    
    // Calculate buffer sizes and initialize them
    for (int bufferIdx = 0; bufferIdx <= maxBufferIndex; ++bufferIdx) {
        size_t bufferSize = 0;
        size_t stride = 0;
        
        // Find the stride for this buffer
        for (const auto& attr : attributes) {
            if (attr.m_bufferIndex == bufferIdx) {
                stride = std::max(stride, attr.m_stride);
            }
        }
        
        if (stride > 0) {
            bufferSize = stride * vertexCount;
            newVertexBuffers[bufferIdx].resize(bufferSize, 0); // Initialize with zeros
        }
    }

    // Copy attribute data from source to destination format
    std::vector<Attribute> newAttributes;
    
    for (const auto& targetAttr : attributes) {
        // Find the corresponding source attribute
        auto sourceIt = std::find_if(m_attributes.begin(), m_attributes.end(),
            [&targetAttr](const Attribute& attr) { 
                return attr.m_type == targetAttr.m_type; 
            });

        if (sourceIt == m_attributes.end()) {
            LOG(WARNING) << "Source attribute not found for type: " << static_cast<int>(targetAttr.m_type);
            continue; // Skip attributes that don't exist in source
        }

        const auto& sourceAttr = *sourceIt;

        // Validate size compatibility
        if (sourceAttr.m_size != targetAttr.m_size) {
            LOG(WARNING) << "Attribute size mismatch for type: " << static_cast<int>(targetAttr.m_type)
                        << " (source: " << sourceAttr.m_size << ", target: " << targetAttr.m_size << ")";
            continue;
        }

        // Get source and destination data pointers
        const uint8_t* srcData = m_vertexBuffers[sourceAttr.m_bufferIndex].data() + sourceAttr.m_offset;
        uint8_t* dstData = newVertexBuffers[targetAttr.m_bufferIndex].data() + targetAttr.m_offset;

        // Copy vertex data for this attribute
        for (size_t vertexIdx = 0; vertexIdx < vertexCount; ++vertexIdx) {
            const uint8_t* srcVertex = srcData + vertexIdx * sourceAttr.m_stride;
            uint8_t* dstVertex = dstData + vertexIdx * targetAttr.m_stride;
            
            std::memcpy(dstVertex, srcVertex, targetAttr.m_size);
        }

        // Add the attribute to the new geometry
        newAttributes.push_back(targetAttr);
    }

    // Copy index buffer if it exists
    std::optional<std::vector<index_t>> newIndexBuffer;
    if (m_indexBuffer.has_value()) {
        newIndexBuffer = *m_indexBuffer; // Copy the index buffer
    }

    return Geometry(
        std::move(newAttributes),
        std::move(newVertexBuffers), 
        std::move(newIndexBuffer)
    );
}

bool okami::FormatsEqual(std::span<Attribute const> a, std::span<Attribute const> b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].m_type != b[i].m_type ||
            a[i].m_size != b[i].m_size ||
            a[i].m_offset != b[i].m_offset ||
            a[i].m_stride != b[i].m_stride ||
            a[i].m_bufferIndex != b[i].m_bufferIndex) {
            return false;
        }
    }
    return true;
}
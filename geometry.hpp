#pragma once

#include <vector>
#include <optional>
#include <span>
#include <filesystem>

#include "common.hpp"
#include "aabb.hpp"

#include <glm/vec3.hpp>
#include <glm/common.hpp>

namespace okami {
    enum class AttributeType {
		Position,
		Normal,
		TexCoord,
		Color,
		Tangent,
		Bitangent,
		Unknown
		// Add more attribute types as needed
	};

	enum class MeshType {
		Static
	};

	template <typename T>
	struct GeometryView {
		T* m_begin;
		T* m_end;

		T* begin() const {
			return m_begin;
		}

		T* end() const {
			return m_end;
		}
	};

    enum class AccessorType {
        Scalar,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat3,
        Mat4
    };

    enum class AccessorComponentType {
        Double,
        Float,
        Int,
        UInt,
        Short,
        UShort,
        Byte,
        UByte
    };

	AccessorType GetAccessorType(AttributeType type);
	AccessorComponentType GetComponentType(AttributeType type);
	uint32_t GetStride(AccessorType type, AccessorComponentType componentType);
	uint32_t GetStride(AttributeType type);

    struct Attribute {
        AttributeType m_type;
        int m_buffer;
        size_t m_offset;

        uint32_t GetStride() const;
    };

	struct IndexInfo {
		AccessorComponentType m_type;
		int m_buffer;
		size_t m_count;
		size_t m_offset;

		uint32_t GetStride() const;
	};
	
	struct GeometryMeshDesc {
		std::vector<Attribute> m_attributes;
		size_t m_vertexCount = 0;
		std::optional<IndexInfo> m_indices;
		MeshType m_type;
		AABB m_aabb;

		size_t GetVertexByteSize() const;
		size_t GetIndexByteSize() const;
		Attribute const* TryGetAttribute(AttributeType type) const;

		inline bool HasIndexBuffer() const {
			return m_indices.has_value();
		}
	};

	void GenerateDefaultAttributeData(
    	std::span<uint8_t> buffer, 
    	AttributeType attrType);

    class RawGeometry {
	private:
        std::vector<std::vector<uint8_t>> m_buffers;
        std::vector<GeometryMeshDesc> m_meshes;

	public:
		RawGeometry() = default;
		OKAMI_NO_COPY(RawGeometry);
		OKAMI_MOVE(RawGeometry);

		inline std::span<std::vector<uint8_t> const> GetBuffers() const {
			return std::span(m_buffers);
		}

        inline std::span<GeometryMeshDesc const> GetMeshes() const {
            return std::span(m_meshes);
        }

		inline size_t GetMeshCount() const {
			return m_meshes.size();
		}

		inline std::span<uint8_t const> GetRawVertexData(int buffer = 0) const {
			if (buffer < 0 || buffer >= m_buffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_buffers[buffer];
		}

		inline std::span<uint8_t> GetRawVertexData(int buffer = 0) {
			if (buffer < 0 || buffer >= m_buffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_buffers[buffer];
		}

		template <typename T>
		std::optional<GeometryView<T>> TryAccess(AttributeType attrType, size_t meshIndex = 0) const {
			if (meshIndex >= m_meshes.size()) {
				return std::nullopt;
            }
            
            const auto& mesh = m_meshes[meshIndex];

            // Find the attribute for the given attribute type
            auto const attribute = mesh.TryGetAttribute(attrType);
            if (!attribute) {
                return std::nullopt;
            }

            // Get the buffer data
            auto data = GetRawVertexData(attribute->m_buffer);

            auto bufferOffset = attribute->m_offset;
            auto stride = attribute->GetStride();

            // Create a view of the buffer data
            return GeometryView<T>(
				reinterpret_cast<T*>(data.data() + bufferOffset),
				reinterpret_cast<T*>(data.data() + bufferOffset + stride * mesh.m_vertexCount)
			);
		}

		template <typename T>
		std::optional<GeometryView<T>> TryAccess(AttributeType attrType, size_t meshIndex = 0) {
			if (meshIndex >= m_meshes.size()) {
				return std::nullopt;
            }
            
            const auto& mesh = m_meshes[meshIndex];
            
            // Find the attribute for the given attribute type
            auto const attribute = mesh.TryGetAttribute(attrType);
            if (!attribute) {
                return std::nullopt;
            }

            // Get the buffer data
            auto data = GetRawVertexData(attribute->m_buffer);

            auto bufferOffset = attribute->m_offset;
            auto stride = attribute->GetStride();

            // Create a view of the buffer data
            return GeometryView<T>{
				reinterpret_cast<T*>(data.data() + bufferOffset),
				reinterpret_cast<T*>(data.data() + bufferOffset + stride * mesh.m_vertexCount)
			};
		}

        static Expected<RawGeometry> LoadGLTF(std::filesystem::path const& path);
	};
}
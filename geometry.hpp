#pragma once

#include <vector>
#include <span>
#include <filesystem>
#include <algorithm>
#include <stdexcept>

#include "common.hpp"

namespace okami {
	using index_t = uint32_t;

	enum class AttributeType {
		Position,
		Normal,
		TexCoord,
		Color,
		Tangent,
		Bitangent,
		// Add more attribute types as needed
	};

	struct Attribute {
		AttributeType m_type; // Type of the attribute
		int m_bufferIndex;
		size_t m_size; // Size in bytes
		size_t m_offset; // Offset in the vertex structure
		size_t m_stride; // Stride in bytes
	};

	template <typename T>
	class GeometryView {
	private:
		using DataType = std::conditional_t<std::is_const_v<T>, uint8_t const, uint8_t>;

		DataType* m_data;
		size_t m_stride; // Stride in bytes
	
	public:
		GeometryView(DataType* data, size_t stride)
			: m_data(data), m_stride(stride) {
		}

		T& operator[](size_t index) {
			return *reinterpret_cast<T*>(m_data + index * m_stride);
		}
	};

	struct GeometryBuffers {
		std::optional<std::span<float const>> positions = std::nullopt;
		std::optional<std::span<float const>> normals = std::nullopt;
		std::optional<std::span<float const>> texCoords = std::nullopt;
		std::optional<std::span<float const>> tangents = std::nullopt;
		std::optional<std::span<float const>> bitangents = std::nullopt;
		std::optional<std::span<index_t const>> indices = std::nullopt;
	};

	class Geometry {
	private:
		std::vector<Attribute> m_attributes;
		std::vector<std::vector<uint8_t>> m_vertexBuffers; // Raw vertex data
		std::optional<std::vector<index_t>> m_indexBuffer;

	public:
		std::span<Attribute const> GetAttributes() const {
			return m_attributes;
		}

		std::span<uint8_t const> GetRawVertexData(int buffer = 0) const {
			if (buffer < 0 || buffer >= m_vertexBuffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_vertexBuffers[buffer];
		}

		std::span<uint8_t> GetRawVertexData(int buffer = 0) {
			if (buffer < 0 || buffer >= m_vertexBuffers.size()) {
				throw std::out_of_range("Invalid buffer index");
			}
			return m_vertexBuffers[buffer];
		}

		bool HasIndexBuffer() const {
			return m_indexBuffer.has_value();
		}

		std::span<index_t const> GetIndexBuffer() const {
			if (!m_indexBuffer) {
				throw std::runtime_error("Index buffer not available");
			}
			return *m_indexBuffer;
		}

		std::span<index_t> GetIndexBuffer() {
			if (!m_indexBuffer) {
				throw std::runtime_error("Index buffer not available");
			}
			return *m_indexBuffer;
		}

		template <typename T>
		GeometryView<T const> GetConstView(AttributeType m_type) const {
			// Find the attribute for the given type
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(),
				[m_type](const Attribute& attr) { return attr.m_type == m_type; });
			if (it == m_attributes.end()) {
				throw std::runtime_error("Attribute type not found");
			}
			if (it->m_size != sizeof(T)) {
				throw std::runtime_error("Attribute size mismatch");
			}
			// Create a view of the vertex data
			return GeometryView<T const>(GetRawVertexData(it->m_bufferIndex).data() + it->m_offset, it->m_stride);
		}

		template <typename T>
		GeometryView<T> GetView(AttributeType m_type) {
			// Find the attribute for the given type
			auto it = std::find_if(m_attributes.begin(), m_attributes.end(),
				[m_type](const Attribute& attr) { return attr.m_type == m_type; });
			if (it == m_attributes.end()) {
				throw std::runtime_error("Attribute type not found");
			}
			if (it->m_size != sizeof(T)) {
				throw std::runtime_error("Attribute size mismatch");
			}
			// Create a view of the vertex data
			return GeometryView<T>(GetRawVertexData(it->m_bufferIndex).data() + it->m_offset, it->m_stride);
		}

		Geometry(std::vector<Attribute> attrs, std::vector<std::vector<uint8_t>> data)
			: m_attributes(std::move(attrs)), m_vertexBuffers(std::move(data)) {
		}

		Geometry(std::vector<Attribute> attrs, std::vector<std::vector<uint8_t>> data, std::optional<std::vector<index_t>> indices)
			: m_attributes(std::move(attrs)), m_vertexBuffers(std::move(data)), m_indexBuffer(std::move(indices)) {
		}

		static Expected<Geometry> LoadGLTF(
			std::filesystem::path const& path,
			std::span<Attribute const> attributes);

		static Expected<Geometry> FromBuffers(
			GeometryBuffers const& buffers,
			std::span<Attribute const> attributes
		);
	};
}
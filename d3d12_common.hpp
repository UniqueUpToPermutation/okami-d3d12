#pragma once

#include <SimpleMath.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <expected>
#include <filesystem>

#include "engine.hpp"
#include "common.hpp"

#include "shaders/common.fxh"

#include <glm/mat4x4.hpp>

#include "camera.hpp"
#include "transform.hpp"
#include "geometry.hpp"

namespace okami {
	using Microsoft::WRL::ComPtr;

	Expected<ComPtr<ID3DBlob>> LoadShaderFromFile(const std::filesystem::path& path);

	template <typename InstanceType>
	class BufferWriteMap {
	private:
		ComPtr<ID3D12Resource> m_resource;
		InstanceType* m_instanceData = nullptr;

	public:
		BufferWriteMap() = default;
		BufferWriteMap(BufferWriteMap&&) = default;
		BufferWriteMap& operator=(BufferWriteMap&&) = default;

		BufferWriteMap(BufferWriteMap const&) = delete;
		BufferWriteMap& operator=(BufferWriteMap const&) = delete;

		inline InstanceType* Data() {
			return m_instanceData;
		}

		static Expected<BufferWriteMap<InstanceType>> Map(
			ID3D12Resource* resource) {
			BufferWriteMap<InstanceType> result;

			if (!resource) {
				return std::unexpected(Error("Resource not initialized"));
			}
			result.m_resource = resource;
			D3D12_RANGE readRange = { 0, 0 }; // No read access
			void* data = nullptr;
			HRESULT hr = result.m_resource->Map(0, &readRange, &data);
			if (FAILED(hr)) {
				return std::unexpected(Error("Failed to map resource"));
			}
			result.m_instanceData = reinterpret_cast<InstanceType*>(data);
			return result;
		}

		InstanceType& operator[](size_t i) {
			return m_instanceData[i];
		}

		InstanceType& At(size_t i) {
			return m_instanceData[i];
		}

		InstanceType& operator*() {
			return *m_instanceData;
		}

		~BufferWriteMap() {
			if (m_resource) {
				m_resource->Unmap(0, nullptr);
			}
		}
	};

	struct Sizer {
	public:
		double m_weightedSize = 0.0;
		double m_sizeDecay = 0.95; // Decay factor for size
		double m_expandFactor = 2.0; // Factor to expand size when needed
		size_t m_currentSize = 0;
		size_t m_minSize = 0;

		inline size_t Reset(size_t m_size) {
			m_currentSize = std::max<size_t>(m_size, m_minSize);
			m_weightedSize = static_cast<double>(m_currentSize);
			return m_currentSize;
		}

		inline std::optional<size_t> GetNextSize(size_t requestedSize) {
			m_weightedSize = (1 - m_sizeDecay) * static_cast<double>(requestedSize) + m_weightedSize * m_sizeDecay;

			if (requestedSize >= m_currentSize) {
				// Buffer is not large enough, expand it
				return Reset(static_cast<size_t>(m_weightedSize * m_expandFactor));
			}
			else if (m_weightedSize <= m_currentSize / (m_expandFactor * m_expandFactor)) {
				if (m_currentSize > m_minSize)
				{
					// Buffer is too large, shrink it
					return Reset(static_cast<size_t>(m_weightedSize * m_expandFactor));
				} else {
					// Can't shrink below minimum size	
					return {};
				}
			}
			else {
				return {};
			}
		}

		inline double operator()() const {
			return m_weightedSize;
		}
	};

	enum class UploadBufferType {
		Constant,
		Structured,
		Vertex,
		Index
	};

	template <typename InstanceType>
	class UploadBuffer {
	private:
		ComPtr<ID3D12Resource> m_buffer;
		Sizer m_sizer;
		UploadBufferType m_type;
		std::wstring_view m_name;

	public:
		UploadBufferType GetType() const {
			return m_type;
		}

		size_t SizeOf(size_t elementCount) const {
			// Structured buffers don't need 256-byte alignment like constant buffers
			return elementCount * sizeof(InstanceType);
		}

		Error Resize(ID3D12Device& device, size_t elementCount) {
			// Calculate the new size of the buffer
			// ensuring alignment to 256 bytes for constant buffers
			size_t newBufferSize = SizeOf(elementCount);
			if (m_type == UploadBufferType::Constant) {
				newBufferSize = (newBufferSize + 255) & ~255;
			}

			// Get the current buffer size
			if (m_buffer) {
				D3D12_RESOURCE_DESC desc = m_buffer->GetDesc();
				size_t currentBufferSize = static_cast<size_t>(desc.Width);

				// If the new size is the same as the current size, no need to resize
				if (newBufferSize == currentBufferSize) {
					return {};
				}
			}

			// Release the current buffer
			m_buffer.Reset();

			if (elementCount == 0) {
				return {};
			}

			// Describe the new buffer resource
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 1;
			heapProperties.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = newBufferSize;
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
				IID_PPV_ARGS(&m_buffer)
			);
			m_buffer->SetName(m_name.data());

			if (FAILED(hr)) {
				return Error("Failed to create structured buffer resource");
			}

			return {};
		}

		static Expected<UploadBuffer<InstanceType>> Create(
			ID3D12Device& device,
			UploadBufferType type,
			std::wstring_view name = L"Upload Buffer",
			size_t elementCount = 1) {
			UploadBuffer<InstanceType> result;
			result.m_type = type;
			result.m_name = name;
			auto err = result.Resize(device, elementCount);
			if (err.IsError()) {
				return std::unexpected(err);
			}
			result.m_sizer.Reset(result.SizeOf(elementCount));
			return result;
		}

		Error Reserve(ID3D12Device& device, size_t elementCount) {
			auto sizeChanged = m_sizer.GetNextSize(SizeOf(elementCount));

			if (sizeChanged) {
				return Resize(device, elementCount);
			}
			else {
				return {};
			}
		}

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const {
			return m_buffer->GetGPUVirtualAddress();
		}

		ID3D12Resource* GetResource() const {
			return m_buffer.Get();
		}

		size_t GetElementCount() const {
			return m_sizer.m_currentSize;
		}

		Expected<BufferWriteMap<InstanceType>> Map() const {
			return BufferWriteMap<InstanceType>::Map(m_buffer.Get());
		}
	};

	inline DirectX::SimpleMath::Matrix GlmToDX(glm::mat4x4 const& mat) {
		return DirectX::SimpleMath::Matrix(
			mat[0][0], mat[0][1], mat[0][2], mat[0][3],
			mat[1][0], mat[1][1], mat[1][2], mat[1][3],
			mat[2][0], mat[2][1], mat[2][2], mat[2][3],
			mat[3][0], mat[3][1], mat[3][2], mat[3][3]);
	}

	hlsl::Camera ToHLSLCamera(
		std::optional<Camera> camera,
		std::optional<Transform> transform,
		int backbufferWidth,
		int backbufferHeight);

	class StaticBuffer {
	private:
		ComPtr<ID3D12Resource> m_buffer;

	public:
		StaticBuffer() = default;
		inline StaticBuffer(ComPtr<ID3D12Resource> buffer)
			: m_buffer(std::move(buffer)) {}

		static Expected<StaticBuffer> Create(
			ID3D12Device& device,
			size_t bufferSize);

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const {
			return m_buffer->GetGPUVirtualAddress();
		}

		ID3D12Resource* GetResource() const {
			return m_buffer.Get();
		}
	};

	size_t GetFormatSize(DXGI_FORMAT format);
}
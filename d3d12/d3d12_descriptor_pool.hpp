#pragma once

#ifdef USE_D3D12

#include <d3d12.h>
#include <wrl/client.h>

#include <set>
#include <expected>

#include "engine.hpp"

namespace okami {
	class DescriptorPool
	{
	public:
		using Handle = uint32_t;

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
		D3D12_DESCRIPTOR_HEAP_FLAGS m_flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		D3D12_CPU_DESCRIPTOR_HANDLE m_heapStartCpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE m_heapStartGpu = {};
		UINT m_heapHandleIncrement = 0;
		UINT m_count = 0; // Total number of descriptors in the heap

		// A set of extra free indices before the beginning of the free block.
		// Reallocate these first.
		std::set<Handle> m_freeIndices;
		// All of the indices starting at this block are free.
		Handle m_freeBlockStart = 0;

	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(Handle handle) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(Handle handle) const;

		inline ID3D12DescriptorHeap* GetHeap() const {
			return m_heap.Get();
		}

		static std::expected<DescriptorPool, Error> Create(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE heapType,
			UINT descriptorCount,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

		std::optional<Handle> TryAlloc();
		Handle Alloc();
		void Free(Handle handle);
		void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

		DescriptorPool() = default;
		OKAMI_MOVE(DescriptorPool);
		OKAMI_NO_COPY(DescriptorPool);
	};
}

#endif
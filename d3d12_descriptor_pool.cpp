#include "assert.hpp"

#include "d3d12_descriptor_pool.hpp"

using namespace okami;

#include <string>

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorPool::GetCpuHandle(Handle handle) const {
	OKAMI_ASSERT(m_heap, "DescriptorPool::GetCpuHandle called on an uninitialized pool");
	return { m_heapStartCpu.ptr + handle * m_heapHandleIncrement };
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorPool::GetGpuHandle(Handle handle) const {
	OKAMI_ASSERT(m_heap, "DescriptorPool::GetGpuHandle called on an uninitialized pool");
	return { m_heapStartGpu.ptr + handle * m_heapHandleIncrement };
}

std::expected<DescriptorPool, Error> DescriptorPool::Create(
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT descriptorCount,
	D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	DescriptorPool pool;
	pool.m_count = descriptorCount;
	pool.m_heapType = heapType;

	// Describe the descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = heapType;
	heapDesc.NumDescriptors = descriptorCount;
	heapDesc.Flags = flags;
	heapDesc.NodeMask = 0;

	// Create the descriptor heap
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pool.m_heap));
	if (FAILED(hr) || !pool.m_heap) {
		return std::unexpected(Error("Failed to create descriptor heap: " + std::to_string(hr)));
	}

	// Initialize heap properties
	pool.m_heapStartCpu = pool.m_heap->GetCPUDescriptorHandleForHeapStart();
	pool.m_heapStartGpu = (flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
		? pool.m_heap->GetGPUDescriptorHandleForHeapStart()
		: D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };
	pool.m_heapHandleIncrement = device->GetDescriptorHandleIncrementSize(heapType);

	return pool;
}

DescriptorPool::Handle DescriptorPool::Alloc()
{
	if (m_freeIndices.empty()) {
		m_freeBlockStart++;
		if (m_freeBlockStart >= m_count) {
			throw std::runtime_error("Descriptor pool exhausted");
		}
		return m_freeBlockStart - 1;
	}
	else {
		auto it = m_freeIndices.begin();
		auto handle = *it;
		m_freeIndices.erase(it);
		return handle;
	}
}

void DescriptorPool::Free(DescriptorPool::Handle handle)
{
	m_freeIndices.emplace(handle);
	while (!m_freeIndices.empty() && *m_freeIndices.rbegin() == m_freeBlockStart - 1) {
		m_freeIndices.erase(*m_freeIndices.rbegin());
		m_freeBlockStart--;
	}
}

void DescriptorPool::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	// Calculate the index from the CPU handle
	auto index = static_cast<Handle>((cpuHandle.ptr - m_heapStartCpu.ptr) / m_heapHandleIncrement);
	if (index < m_count) {
		Free(index);
	}
	else {
		throw std::out_of_range("Descriptor handle out of range");
	}
}
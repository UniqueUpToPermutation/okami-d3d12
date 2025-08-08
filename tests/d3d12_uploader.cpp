#include "d3d12_utils.hpp"

#include "../d3d12_upload.hpp"

#include <array>

using namespace okami;

TEST_F(D3D12Test, SetUpAndTearDown) {
}

constexpr UINT64 kNumDummyFloats = 16;

TEST_F(D3D12Test, UploaderSimple) {
    // Create a simple uploader
    auto uploaderResult = GpuUploader::Create(*m_device.Get());
    ASSERT_TRUE(uploaderResult.has_value()) << "Failed to create GPU uploader: " << uploaderResult.error().Str();
    
    GpuUploader uploader = std::move(uploaderResult.value());

    ComPtr<ID3D12Resource> readOnlyBuffer = nullptr;
    
    // Submit a dummy task
    class DummyTask : public GpuUploaderTask {
    private:
        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12Resource> m_uploadBuffer;
        ComPtr<ID3D12Resource>& m_readOnlyBuffer;

    public:
        DummyTask(ComPtr<ID3D12Device> device, ComPtr<ID3D12Resource>& readOnlyBuffer)
            : m_device(std::move(device)), m_readOnlyBuffer(readOnlyBuffer) {}

        Error Execute(ID3D12Device& device,
            ID3D12GraphicsCommandList& commandList) override {
            // Describe the new buffer resource
			D3D12_HEAP_PROPERTIES uploadHeap = {};
			uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
			uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			uploadHeap.CreationNodeMask = 1;
			uploadHeap.VisibleNodeMask = 1;

            D3D12_HEAP_PROPERTIES readOnlyHeap = {};
            readOnlyHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
            readOnlyHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            readOnlyHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            readOnlyHeap.CreationNodeMask = 1;
            readOnlyHeap.VisibleNodeMask = 1;

			D3D12_RESOURCE_DESC resourceDesc = {};
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = kNumDummyFloats * sizeof(float);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc.Count = 1;
			resourceDesc.SampleDesc.Quality = 0;
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            auto hr = m_device->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                nullptr,
                IID_PPV_ARGS(&m_uploadBuffer)
            );
            if (FAILED(hr)) {
                return Error("Failed to create upload buffer resource");
            }

            hr = m_device->CreateCommittedResource(
                &readOnlyHeap,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&m_readOnlyBuffer)
            );
            if (FAILED(hr)) {
                return Error("Failed to create read-only buffer resource");
            }

            // Fill the upload buffer with dummy data
            float* uploadBufferData = nullptr;
            m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadBufferData));
            if (!uploadBufferData) {
                return Error("Failed to map upload buffer");
            }
            for (int i = 0; i < kNumDummyFloats; ++i) {
                uploadBufferData[i] = static_cast<float>(i);
            }
            m_uploadBuffer->Unmap(0, nullptr);

            commandList.CopyResource(
                m_readOnlyBuffer.Get(),
                m_uploadBuffer.Get()
            );

            return {};
        }
        
        Error Finalize() override {
            m_uploadBuffer.Reset();
            return {};
        }
    };
    
    uploader.SubmitTask(std::make_unique<DummyTask>(m_device, readOnlyBuffer));
    
    std::optional<GpuUploaderCommandList> commandListOpt = uploader.GetExecutableCommandListIfAny();
    while (!commandListOpt.has_value()) {
        // Wait for a command list to become available
        Sleep(1);
        commandListOpt = uploader.GetExecutableCommandListIfAny();
    }

    std::array<ID3D12CommandList*, 1> copyCommandLists = {
        commandListOpt->m_commandList.Get(),
    };
    m_copyQueue->ExecuteCommandLists(1, copyCommandLists.data());
	m_copyQueue->Signal(commandListOpt->m_fenceToSignal.Get(), commandListOpt->m_fenceValue);

    // Finalize tasks
    while (uploader.FetchAndFinalizeTasks() > 0) {
        // Wait for tasks to finalize
        Sleep(1);
    }

    // Transition resource
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // Transition readOnlyBuffer from COPY_DEST to VERTEX_AND_CONSTANT_BUFFER
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        readOnlyBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
    );
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->Close();

    std::array<ID3D12CommandList*, 1> commandLists = {
        m_commandList.Get()
    };
    m_commandQueue->ExecuteCommandLists(1, commandLists.data());
	m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue);

    uploader.Stop();
}
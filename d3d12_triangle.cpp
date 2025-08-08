#include "d3d12_triangle.hpp"
#include "paths.hpp"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <directx/d3dx12.h>
#include <fstream>
#include <filesystem>
#include <vector>

#include "d3d12_common.hpp"

#include <glog/logging.h>

using namespace okami;

Expected<ComPtr<ID3D12RootSignature>> TriangleRenderer::CreateRootSignature(ID3D12Device& device) {
    ComPtr<ID3D12RootSignature> rootSignature;

    // Create root parameters: constant buffer for globals, shader resource view for structured buffer
    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    
    rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsShaderResourceView(1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);

    // Create root signature description
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr)) {
        std::string errorMsg = "Failed to serialize root signature";
        if (error) {
            errorMsg += ": " + std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize());
        }
        return std::unexpected(errorMsg);
    }

    hr = device.CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        return std::unexpected(Error("Failed to create root signature"));
    }

    return rootSignature; // Success
}

Error TriangleRenderer::Startup(
    ID3D12Device& device,
    DirectX::RenderTargetState rts,
    int bufferCount) {

    // Load vertex shader
    auto vertexShader = LoadShaderFromFile(GetShaderPath("triangle_vs.cso"));
    if (!vertexShader) {
        return vertexShader.error();
    }

    // Load pixel shader
	auto pixelShader = LoadShaderFromFile(GetShaderPath("triangle_ps.cso"));
    if (!pixelShader) {
        return pixelShader.error();
    }

    // Create root signature
	auto rootSignature = CreateRootSignature(device);
    if (!rootSignature) {
        return rootSignature.error();
    }
	m_rootSignature = std::move(rootSignature.value());

    // Define the graphics pipeline state object description
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { nullptr, 0 }; // No input layout needed for vertex shader that generates vertices
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = rts.numRenderTargets;
    
    for (UINT i = 0; i < rts.numRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = rts.rtvFormats[i];
    }
    
    psoDesc.DSVFormat = rts.dsvFormat;
    psoDesc.SampleDesc = rts.sampleDesc;

    // Create the graphics pipeline state object
    HRESULT hr = device.CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
    if (FAILED(hr)) {
        return Error("Failed to create graphics pipeline state");
    }

    for (int i = 0; i < bufferCount; ++i) {
        auto shaderConstants = ConstantBuffer<hlsl::Globals>::Create(device);
        if (!shaderConstants) {
            return shaderConstants.error();
        }
        // Start with an empty structued buffer, because this renderer
		// will probably not be used
		auto instanceBuffer = StructuredBuffer<hlsl::Instance>::Create(device, 0);
        if (!instanceBuffer) {
            return instanceBuffer.error();
		}

        m_perFrameData.emplace_back(PerFrameData{
			.m_globalConstants = std::move(shaderConstants.value()),
			.m_instanceBuffer = std::move(instanceBuffer.value())
        });
	}

    return {};
}

Error TriangleRenderer::Render(
    ID3D12Device& device,
    ID3D12GraphicsCommandList& commandList,
    hlsl::Globals const& globals,
    IStorageAccessor<Transform> const& transforms) {
    auto dummyTriangles = m_dummyTriangleStorage.GetStorage<DummyTriangleComponent>();

    if (dummyTriangles.empty()) {
        return {};
	}

    // Set the pipeline state
    commandList.SetPipelineState(m_pipelineState.Get());

    // Set the root signature
    commandList.SetGraphicsRootSignature(m_rootSignature.Get());

    // Set primitive topology
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set constant buffer views for globals
    {
        auto& globalConstants = m_perFrameData[m_currentBuffer].m_globalConstants;
        auto map = globalConstants.Map();
        if (!map) {
            return Error("Failed to map global constants");
		}
        **map = globals;
        commandList.SetGraphicsRootConstantBufferView(0, globalConstants.GetGPUAddress());
    }

	// Resize structured buffer if necessary to fit all triangles
	auto& instanceBuffer = m_perFrameData[m_currentBuffer].m_instanceBuffer;
    if (instanceBuffer.Reserve(device, dummyTriangles.size()).IsError()) {
		return Error("Failed to reserve structured buffer for dummy triangles");
    }

    // Map and fill instance data
    auto map = instanceBuffer.Map();
    if (!map) {
        return Error("Failed to map structured buffer");
	}

    // Fill instance data for all triangles
    int index = 0;
    for (auto const& [entity, triangle] : dummyTriangles) {
        // Assuming each triangle has a transform component
        auto transform = transforms.GetOr(entity, Transform::Identity());

        auto worldMatrix = transform.AsMatrix();

        // Set instance data
        map->At(index) = hlsl::Instance{
            .m_worldMatrix = worldMatrix,
            .m_worldInverseTransposeMatrix = glm::inverse(glm::transpose(worldMatrix))
        };
        ++index;
	}

    // Set structured buffer and draw all instances in one call
    commandList.SetGraphicsRootShaderResourceView(1, instanceBuffer.GetGPUAddress());
    commandList.DrawInstanced(3, static_cast<UINT>(dummyTriangles.size()), 0, 0);

    m_currentBuffer = (m_currentBuffer + 1) % m_perFrameData.size();

    return {};
}

void TriangleRenderer::Shutdown() {
    m_rootSignature.Reset();
    m_pipelineState.Reset();
    m_perFrameData.clear();
    m_dummyTriangleStorage.Clear();
}
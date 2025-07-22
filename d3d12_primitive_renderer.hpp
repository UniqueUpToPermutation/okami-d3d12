#pragma once

#include <d3d12.h>

#include <directxtk12/PrimitiveBatch.h>
#include <directxtk12/Effects.h>
#include <directxtk12/CommonStates.h>
#include <directxtk12/VertexTypes.h>
#include <directxtk12/RenderTargetState.h>
#include <directxtk12/SimpleMath.h>

#include <memory>
#include <optional>

namespace Math = DirectX::SimpleMath;

namespace okami {
	class PrimitiveRenderer {
	public:
		std::unique_ptr<DirectX::BasicEffect> m_triangleEffect;
		std::unique_ptr<DirectX::BasicEffect> m_lineEffect;
		std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;

		static std::optional<PrimitiveRenderer> Create(
			ID3D12Device* device,
			DirectX::RenderTargetState rts);

		void BeginTriangles(
			ID3D12GraphicsCommandList* commandList,
			const Math::Matrix& world = Math::Matrix::Identity,
			const Math::Matrix& view = Math::Matrix::Identity,
			const Math::Matrix& projection = Math::Matrix::Identity);
		void BeginLines(
			ID3D12GraphicsCommandList* commandList,
			const Math::Matrix& world = Math::Matrix::Identity,
			const Math::Matrix& view = Math::Matrix::Identity,
			const Math::Matrix& projection = Math::Matrix::Identity);

		void DrawTriangles(ID3D12GraphicsCommandList* commandList, const DirectX::VertexPositionColor* vertices, size_t vertexCount);
		void DrawLines(ID3D12GraphicsCommandList* commandList, const DirectX::VertexPositionColor* vertices, size_t vertexCount);

		void End();
	};
}
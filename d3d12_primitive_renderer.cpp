#include "d3d12_primitive_renderer.hpp"

using namespace okami;
using namespace DirectX;
using namespace DirectX::SimpleMath;

std::optional<PrimitiveRenderer> PrimitiveRenderer::Create(
	ID3D12Device* device,
	DirectX::RenderTargetState rts) {
	PrimitiveRenderer renderer;

	// Create primitive batch
	renderer.m_primitiveBatch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(device, 4096U);

	// Create effects with depth buffer support
	EffectPipelineStateDescription trianglePd(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rts,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	renderer.m_triangleEffect = std::make_unique<BasicEffect>(device,
		EffectFlags::VertexColor,
		trianglePd);

	EffectPipelineStateDescription linePd(
		&VertexPositionColor::InputLayout,
		CommonStates::Opaque,
		CommonStates::DepthDefault,
		CommonStates::CullNone,
		rts,
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

	renderer.m_lineEffect = std::make_unique<BasicEffect>(device,
		EffectFlags::VertexColor,
		linePd);

	return renderer;
}

void PrimitiveRenderer::BeginTriangles(
	ID3D12GraphicsCommandList* commandList,
	const Matrix& world,
	const Matrix& view,
	const Matrix& projection) {

	// Set transform matrices for both effects
	m_triangleEffect->SetWorld(world);
	m_triangleEffect->SetView(view);
	m_triangleEffect->SetProjection(projection);

	m_triangleEffect->Apply(commandList);
	m_primitiveBatch->Begin(commandList);
}

void PrimitiveRenderer::BeginLines(
	ID3D12GraphicsCommandList* commandList,
	const Matrix& world,
	const Matrix& view,
	const Matrix& projection) {

	m_lineEffect->SetWorld(world);
	m_lineEffect->SetView(view);
	m_lineEffect->SetProjection(projection);

	m_lineEffect->Apply(commandList);
	m_primitiveBatch->Begin(commandList);
}

void PrimitiveRenderer::DrawTriangles(ID3D12GraphicsCommandList* commandList, const VertexPositionColor* vertices, size_t vertexCount) {
	m_primitiveBatch->Draw(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, vertices, vertexCount);
}

void PrimitiveRenderer::DrawLines(ID3D12GraphicsCommandList* commandList, const VertexPositionColor* vertices, size_t vertexCount) {
	m_primitiveBatch->Draw(D3D_PRIMITIVE_TOPOLOGY_LINELIST, vertices, vertexCount);
}

void PrimitiveRenderer::End() {
	m_primitiveBatch->End();
}
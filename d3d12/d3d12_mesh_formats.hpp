#pragma once

#ifdef USE_D3D12

#include "../geometry.hpp"

#include <d3d12.h>
#include <array>
#include <string_view>

namespace okami {
    constexpr std::array<AttributeType, 4> kStaticMeshAttributes = {
        AttributeType::Position,
        AttributeType::Normal,
        AttributeType::TexCoord,
        AttributeType::Tangent
    };

	using MeshRequirements = std::unordered_map<MeshType, std::vector<AttributeType>>;

    MeshRequirements GetD3D12MeshRequirements();

    DXGI_FORMAT GetD3D12Format(AccessorType type, AccessorComponentType componentType);
    DXGI_FORMAT GetD3D12Format(AttributeType type);
    D3D12_INPUT_ELEMENT_DESC GetD3D12InputElementDesc(AttributeType type, UINT inputSlot);
    std::string_view GetD3D12SemanticName(AttributeType type);

    template <size_t N>
    constexpr std::array<D3D12_INPUT_ELEMENT_DESC, N> GetD3D12InputLayout(
        std::array<AttributeType, N> const& attributes) {
        std::array<D3D12_INPUT_ELEMENT_DESC, N> layout = {};
        for (uint32_t i = 0; i < N; ++i) {
            layout[i] = GetD3D12InputElementDesc(attributes[i], i);
        }
        return layout;
    }
}

#endif
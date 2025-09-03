#ifdef USE_D3D12

#include "d3d12_mesh_formats.hpp"

#include <d3d12.h>

#include <array>
#include <span>

using namespace okami;

DXGI_FORMAT okami::GetD3D12Format(AccessorType type, AccessorComponentType componentType) {
    if (type == AccessorType::Scalar) {
        switch (componentType) {
        case AccessorComponentType::Float: return DXGI_FORMAT_R32_FLOAT;
        case AccessorComponentType::Int: return DXGI_FORMAT_R32_SINT;
        case AccessorComponentType::UInt: return DXGI_FORMAT_R32_UINT;
        case AccessorComponentType::Short: return DXGI_FORMAT_R16_SINT;
        case AccessorComponentType::UShort: return DXGI_FORMAT_R16_UINT;
        case AccessorComponentType::Byte: return DXGI_FORMAT_R8_SINT;
        case AccessorComponentType::UByte: return DXGI_FORMAT_R8_UINT;
        }
    }
    else if (type == AccessorType::Vec2) {
        switch (componentType) {
        case AccessorComponentType::Float: return DXGI_FORMAT_R32G32_FLOAT;
        case AccessorComponentType::Int: return DXGI_FORMAT_R32G32_SINT;
        case AccessorComponentType::UInt: return DXGI_FORMAT_R32G32_UINT;
        case AccessorComponentType::Short: return DXGI_FORMAT_R16G16_SINT;
        case AccessorComponentType::UShort: return DXGI_FORMAT_R16G16_UINT;
        case AccessorComponentType::Byte: return DXGI_FORMAT_R8G8_SINT;
        case AccessorComponentType::UByte: return DXGI_FORMAT_R8G8_UINT;
        }
    } 
    else if (type == AccessorType::Vec3) {
        switch (componentType) {
        case AccessorComponentType::Float: return DXGI_FORMAT_R32G32B32_FLOAT;
        case AccessorComponentType::Int: return DXGI_FORMAT_R32G32B32_SINT;
        case AccessorComponentType::UInt: return DXGI_FORMAT_R32G32B32_UINT;
        }
    }
    else if (type == AccessorType::Vec4) {
        switch (componentType) {
        case AccessorComponentType::Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case AccessorComponentType::Int: return DXGI_FORMAT_R32G32B32A32_SINT;
        case AccessorComponentType::UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
        case AccessorComponentType::Short: return DXGI_FORMAT_R16G16B16A16_SINT;
        case AccessorComponentType::UShort: return DXGI_FORMAT_R16G16B16A16_UINT;
        case AccessorComponentType::Byte: return DXGI_FORMAT_R8G8B8A8_SINT;
        case AccessorComponentType::UByte: return DXGI_FORMAT_R8G8B8A8_UINT;
        }
    }
    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT okami::GetD3D12Format(AttributeType type) {
    return GetD3D12Format(GetAccessorType(type), GetComponentType(type));
}

std::string_view okami::GetD3D12SemanticName(AttributeType type) {
    switch (type) {
    case AttributeType::Position: return "POSITION";
    case AttributeType::Normal: return "NORMAL";
    case AttributeType::TexCoord: return "TEXCOORD";
    case AttributeType::Color: return "COLOR";
    case AttributeType::Tangent: return "TANGENT";
    // Add more mappings as needed
    default: return "UNKNOWN";
    }
}

D3D12_INPUT_ELEMENT_DESC okami::GetD3D12InputElementDesc(AttributeType type, UINT inputSlot) {
    D3D12_INPUT_ELEMENT_DESC desc = {};
    desc.SemanticName = GetD3D12SemanticName(type).data();
    desc.SemanticIndex = 0;
    desc.Format = GetD3D12Format(type);
    desc.InputSlot = inputSlot;
    desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    desc.InstanceDataStepRate = 0;
    return desc;
}

MeshRequirements okami::GetD3D12MeshRequirements() {
    static MeshRequirements reqs = {
        { MeshType::Static, std::vector(kStaticMeshAttributes.begin(), kStaticMeshAttributes.end()) }
    };
    return reqs;
}

#endif
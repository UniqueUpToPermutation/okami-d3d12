#include "common.fxh"

cbuffer GlobalsBuffer : register(b0)
{
    Globals globals;
};

StructuredBuffer<Instance> LocalsBuffer : register(t1);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    uint instanceID : SV_InstanceID;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

PSInput VSMain(VSInput input)
{
    float4x4 worldMatrix = 
        LocalsBuffer[input.instanceID].m_worldMatrix;
    float4x4 worldInverseTransposeMatrix = 
        LocalsBuffer[input.instanceID].m_worldInverseTransposeMatrix;
    
    // Apply instance transformation
    float4 worldPosition = mul(worldMatrix, float4(input.position, 1.0));

    PSInput result;
    result.position = mul(globals.m_camera.m_viewProjectionMatrix, worldPosition);
    result.worldPosition = worldPosition.xyz;
    result.uv = input.uv;

    // Compute normal, tangent, and bitangent
    float3 normal = mul(worldInverseTransposeMatrix, float4(input.normal, 0.0)).xyz;
    float3 tangent = mul(worldMatrix, float4(input.tangent, 0.0)).xyz;
    normal = normalize(normal);
    tangent = normalize(tangent);
    float3 bitagent = cross(normal, tangent);
    
    result.normal = normal;
    result.tangent = tangent;
    result.bitangent = bitagent;
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(0.5 * input.normal + 0.5, 1.0);
}

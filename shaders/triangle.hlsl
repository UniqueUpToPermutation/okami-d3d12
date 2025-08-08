#include "common.fxh"

cbuffer GlobalsBuffer : register(b0)
{
    Globals globals;
};

StructuredBuffer<Instance> LocalsBuffer : register(t1);

struct VSInput
{
    uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    float3 position;
    float3 color;

    if (input.vertexID == 0)
    {
        position = float3(0.0f, sqrt(3.0) / 4.0f, 0.0f);
        color = float3(1.0f, 0.0f, 0.0f); // Red
    }
    else if (input.vertexID == 1)
    {
        position = float3(-0.5f, -sqrt(3.0) / 4.0f, 0.0f);
        color = float3(0.0f, 0.0f, 1.0f); // Blue
    }
    else // input.vertexID == 2
    {
        position = float3(0.5f, -sqrt(3.0) / 4.0f, 0.0f);
        color = float3(0.0f, 1.0f, 0.0f); // Green
    }

    // Apply instance transformation
    float4 worldPosition = mul(LocalsBuffer[input.instanceID].m_worldMatrix, float4(position, 1.0));

    PSInput result;
    result.position = mul(globals.m_camera.m_viewProjectionMatrix, worldPosition);
    result.color = float4(color, 1.0);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}

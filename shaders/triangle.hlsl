#include "common.fxh"

cbuffer GlobalsBuffer : register(b0)
{
    Globals globals;
};

cbuffer LocalsBuffer : register(b1)
{
    Instance locals;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(uint vertexID : SV_VertexID)
{
    float3 position;
    float3 color;
    
    if (vertexID == 0)
    {
        position = float3(-1.0f, -1.0f, 0.0f);
        color = float3(1.0f, 0.0f, 0.0f); // Red
    }
    else if (vertexID == 1)
    {
        position = float3(0.0f, 1.0f, 0.0f);
        color = float3(0.0f, 1.0f, 0.0f); // Green
    }
    else // vertexID == 2
    {
        position = float3(1.0f, -1.0f, 0.0f);
        color = float3(0.0f, 0.0f, 1.0f); // Blue
    }
    
    float4 worldPosition = mul(locals.worldMatrix, float4(position, 1.0));
    
    PSInput result;
    result.position = mul(globals.viewProjectionMatrix, worldPosition);
    result.color = float4(color, 1.0);
    
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}


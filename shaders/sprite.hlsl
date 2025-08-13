#include "sprite.fxh"
#include "common.fxh"

cbuffer GlobalsBuffer : register(b0)
{
    Globals globals;
};

Texture2D<float4> SpriteTexture : register(t1);

SamplerState SpriteSampler : register(s0);

struct GSInput
{
    float4 position : SV_POSITION;
    float4 dx : COLOR0;
    float4 dy : COLOR1;
    float4 color : COLOR2;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

GSInput VSMain(SpriteInstance input)
{
    // Use instance data directly from vertex input
    GSInput result;
    result.position = mul(globals.m_camera.m_viewProjectionMatrix, 
        float4(input.position, 1.0));

    float cr = cos(input.rotation);
    float sr = sin(input.rotation);
    float2x2 rotationMatrix = float2x2(cr, -sr, sr, cr);

    float2 dx = mul(rotationMatrix, float2(input.size.x, 0.0));
    float2 dy = mul(rotationMatrix, float2(0.0, input.size.y));

    result.dx = mul(globals.m_camera.m_viewProjectionMatrix, float4(dx, 0.0, 0.0));
    result.dy = mul(globals.m_camera.m_viewProjectionMatrix, float4(dy, 0.0, 0.0));
    
    // We render the sprite from the bottom left corner
    float2 pd = input.origin / input.size;
    result.position -= pd.x * result.dx + pd.y * result.dy;
    
    result.color = input.color;

    return result;
}

[maxvertexcount(6)]
void GSMain(point GSInput input[1], inout TriangleStream<PSInput> output) {
    PSInput v0;
    v0.position = input[0].position;
    v0.uv = float2(0.0, 1.0);
    v0.color = input[0].color;

    PSInput v1;
    v1.position = input[0].position + input[0].dx;
    v1.uv = float2(1.0, 1.0);
    v1.color = input[0].color;

    PSInput v2;
    v2.position = input[0].position + input[0].dy;
    v2.uv = float2(0.0, 0.0);
    v2.color = input[0].color;

    PSInput v3;
    v3.position = input[0].position + input[0].dx + input[0].dy;
    v3.uv = float2(1.0, 0.0);
    v3.color = input[0].color;

    output.Append(v0);
    output.Append(v1);
    output.Append(v2);

    output.Append(v1);
    output.Append(v2);
    output.Append(v3);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return SpriteTexture.Sample(SpriteSampler, input.uv) * input.color;
}

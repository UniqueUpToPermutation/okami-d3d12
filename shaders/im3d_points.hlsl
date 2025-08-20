#include "common.fxh"
#include "im3d_common.hlsl"

#define Z_FLIP -1.0

cbuffer cbContextData : register(b0)
{
    Globals gGlobals;
};

VS_OUTPUT VSMain(VS_INPUT vs_input) 
{
    VS_OUTPUT result;

    result.m_color = vs_input.m_color;
    result.m_position = apply(
            gGlobals.mCamera.mViewProj,
            float4(vs_input.m_positionSize.xyz, 1.0)
    );
    result.m_size = max(vs_input.m_positionSize.w, kAntialiasing);

    return result;
}

[maxvertexcount(4)]
void GSMain(in point VS_OUTPUT gs_input[1], inout TriangleStream<GS_OUTPUT> result)
{
    GS_OUTPUT ret;
    
    float2 scale = 1.0 / gGlobals.mCamera.mViewport * gs_input[0].m_size;
    ret.m_size  = gs_input[0].m_size;
    ret.m_color = gs_input[0].m_color;
    
    ret.m_position = float4(gs_input[0].m_position.xy + float2(-1.0, -1.0) * scale * gs_input[0].m_position.w, gs_input[0].m_position.zw);
    ret.m_uv = float2(0.0, 0.0);
    result.Append(ret);
    
    ret.m_position = float4(gs_input[0].m_position.xy + float2(-1.0,  1.0) * scale * gs_input[0].m_position.w, gs_input[0].m_position.zw);
    ret.m_uv = float2(0.0, 1.0);
    result.Append(ret);
    
    ret.m_position = float4(gs_input[0].m_position.xy + float2( 1.0, -1.0) * scale * gs_input[0].m_position.w, gs_input[0].m_position.zw);
    ret.m_uv = float2(1.0, 0.0);
    result.Append(ret);
    
    ret.m_position = float4(gs_input[0].m_position.xy + float2( 1.0,  1.0) * scale * gs_input[0].m_position.w, gs_input[0].m_position.zw);
    ret.m_uv = float2(1.0, 1.0);
    result.Append(ret);
}

float4 PSMain(in GS_OUTPUT ps_input) : SV_TARGET {
    float4 result = ps_input.m_color;
    float d = length(ps_input.m_uv - float2(0.5, 0.5));
    d = smoothstep(0.5, 0.5 - (kAntialiasing / ps_input.m_size), d);
    result.a *= d;
    return result;
}
#include "common.fxh"
#include "im3d_common.hlsl"

cbuffer cbContextData : register(b0)
{
    Globals gGlobals;
};

VS_OUTPUT VSMain(VS_INPUT vs_input) 
{  
	VS_OUTPUT result;
	result.m_color = vs_input.m_color;
	result.m_position = mul(gGlobals.m_camera.m_viewProjectionMatrix, 
		float4(vs_input.m_positionSize.xyz, 1.0));
   	result.m_color.a *= smoothstep(0.0, 1.0, vs_input.m_positionSize.w / kAntialiasing);
    result.m_size = max(vs_input.m_positionSize.w, kAntialiasing);
	return result;
}

float4 PSMain(VS_OUTPUT ps_input) : SV_TARGET {
	return ps_input.m_color;
}
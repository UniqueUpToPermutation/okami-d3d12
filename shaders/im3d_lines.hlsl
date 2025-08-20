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
	result.m_position = mul(gGlobals.m_camera.m_viewMatrix, 
		float4(vs_input.m_positionSize.xyz, 1.0));
   	result.m_color.a *= smoothstep(0.0, 1.0, vs_input.m_positionSize.w / kAntialiasing);
    result.m_size = max(vs_input.m_positionSize.w, kAntialiasing);
	return result;
}

[maxvertexcount(4)]
void GSMain(line VS_OUTPUT gs_input[2], inout TriangleStream<GS_OUTPUT> result) {

	float2 viewport = gGlobals.mCamera.mViewport;

	float4 p0 = gs_input[0].m_position;
    float4 p1 = gs_input[1].m_position;
    
    p0.z -= Z_FLIP * gGlobals.mCamera.mNearZ;
    p1.z -= Z_FLIP * gGlobals.mCamera.mNearZ;
    
    // Clip both points to viewable camera space.
    if (Z_FLIP * p0.z < 0) {
    	float t = p1.z / (p1.z - p0.z);
    	p0 = t * p0 + (1 - t) * p1;
    }
    if (Z_FLIP * p1.z < 0) {
    	float t = p0.z / (p0.z - p1.z);
    	p1 = t * p1 + (1 - t) * p0;
    }
    
    p0.z += Z_FLIP * gGlobals.mCamera.mNearZ;
    p1.z += Z_FLIP * gGlobals.mCamera.mNearZ;
    
    // Apply projection transformation
    p0 = mul(gGlobals.mCamera.mProj, p0);
    p1 = mul(gGlobals.mCamera.mProj, p1);
    
    float2 dir = p0.xy / p0.w - p1.xy / p1.w;
    // correct for aspect ratio
    dir = normalize(float2(dir.x, dir.y * viewport.y / viewport.x)); 
    float2 tng0 = float2(-dir.y, dir.x);
    
    float2 tng1 = tng0 * gs_input[1].m_size / viewport;
    tng0 = tng0 * gs_input[0].m_size / viewport;
    
    float4 v00 = p0;
    float4 v01 = p0;
    float4 v10 = p1;
    float4 v11 = p1;
    
    v00.xy += tng0 * p0.w;
    v01.xy -= tng0 * p0.w;
    v10.xy += tng1 * p1.w;
    v11.xy -= tng1 * p1.w;

	GS_OUTPUT gs0;
	gs0.m_color = gs_input[0].m_color;
	gs0.m_size = gs_input[0].m_size;
	gs0.m_uv = float2(0.0, 0.0);
	gs0.m_edgeDistance = 0.0;
	
	GS_OUTPUT gs1;
	gs1.m_color = gs_input[1].m_color;
	gs1.m_size = gs_input[1].m_size;
	gs1.m_uv = float2(1.0, 1.0);
	gs1.m_edgeDistance = 0.0;

	gs0.m_position = v00;
	gs0.m_edgeDistance = gs_input[0].m_size;
	result.Append(gs0);
	
	gs0.m_position = v01;
	gs0.m_edgeDistance = -gs_input[0].m_size;
	result.Append(gs0);
	
	gs1.m_position = v10;
	gs1.m_edgeDistance = gs_input[1].m_size;
	result.Append(gs1);
	
	gs1.m_position = v11;
	gs1.m_edgeDistance = -gs_input[1].m_size;
	result.Append(gs1);
}

float4 PSMain(GS_OUTPUT ps_input) : SV_TARGET {
	float4 result = ps_input.m_color;
    float d = abs(ps_input.m_edgeDistance) / ps_input.m_size;
    d = smoothstep(1.0, 1.0 - (kAntialiasing / ps_input.m_size), d);
    result.a *= d;
    return result;
}
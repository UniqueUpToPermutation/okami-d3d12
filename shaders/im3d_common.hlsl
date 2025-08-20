#define kAntialiasing 2.0

struct GS_OUTPUT
{
	float4 m_position     : SV_POSITION;
	float4 m_color        : COLOR;
	float2 m_uv           : TEXCOORD;
	float  m_size         : SIZE;
	float  m_edgeDistance : EDGE_DISTANCE;
};

struct VS_OUTPUT {
	float4 m_position     : SV_POSITION;
	float  m_size		  : SIZE;
	float4 m_color        : COLOR;
};

struct VS_INPUT
{
    float4 m_positionSize : ATTRIB0;
    float4 m_color        : ATTRIB1;
};
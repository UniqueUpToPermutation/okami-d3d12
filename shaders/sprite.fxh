#ifndef SPRITE_FXH
#define SPRITE_FXH

#include "cpp.fxh"

BEGIN_CPP_INTERFACE__

struct SpriteInstance
{
    float3 position      __ATTRIB(POSITION0);
    float rotation       __ATTRIB(ROTATION0);
    float2 size          __ATTRIB(SIZE0);
    float2 uv0           __ATTRIB(TEXCOORD0);
    float2 uv1           __ATTRIB(TEXCOORD1);
    float2 origin        __ATTRIB(ORIGIN0);
    float4 color         __ATTRIB(COLOR0);
};

END_CPP_INTERFACE__

#endif
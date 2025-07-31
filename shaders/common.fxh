#ifndef COMMON_FXH
#define COMMON_FXH

#include "cpp.fxh"

BEGIN_CPP_INTERFACE__

struct Globals
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjectionMatrix;
};

struct Instance
{
    float4x4 worldMatrix;
};

END_CPP_INTERFACE__

#endif
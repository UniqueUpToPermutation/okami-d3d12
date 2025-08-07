#ifndef COMMON_FXH
#define COMMON_FXH

#include "cpp.fxh"

BEGIN_CPP_INTERFACE__

struct Camera
{
    float4x4 m_viewMatrix;
    float4x4 m_projectionMatrix;
    float4x4 m_viewProjectionMatrix;
};

struct Globals
{
    Camera m_camera;
};

struct Instance
{
    float4x4 m_worldMatrix;
    float4x4 m_worldInverseTransposeMatrix;
};

END_CPP_INTERFACE__

#endif
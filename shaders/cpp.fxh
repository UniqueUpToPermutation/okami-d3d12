#ifndef CPP_FXH
#define CPP_FXH

#ifdef __cplusplus 
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace hlsl {
    using float4x4 = glm::mat4;

    using float4 = glm::vec4;
    using float3 = glm::vec3;
    using float2 = glm::vec2;
}
#endif

#ifdef __cplusplus
#define BEGIN_CPP_INTERFACE__ namespace hlsl {
#define END_CPP_INTERFACE__ }
#else
#define BEGIN_CPP_INTERFACE__
#define END_CPP_INTERFACE__
#endif

#endif
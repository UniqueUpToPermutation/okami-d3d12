#ifndef CPP_FXH
#define CPP_FXH

#ifdef __cplusplus 
#include <glm/mat4x4.hpp>

namespace hlsl {
    using float4x4 = glm::mat4;
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
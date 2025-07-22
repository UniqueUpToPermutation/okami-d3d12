# CMakeList.txt : CMake project for Engine, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# vcpkg integration (optional, recommended)
# To use vcpkg, configure CMake with:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>/scripts/buildsystems/vcpkg.cmake"
# Or set the VCPKG_ROOT environment variable.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Engine")

# Find packages via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

# Automatically add all .cpp and .hpp files in the project directory
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

add_executable (Engine ${ENGINE_SOURCES} ${IMGUI_IMPL_SOURCES})

# Link vcpkg libraries with correct target names
# glfw (not glfw3::glfew), imgui::imgui, Microsoft::DirectXTK12
# Also link Direct3D12 system library for native D3D12 API usage
# See vcpkg CMake integration output for details
# https://github.com/microsoft/vcpkg/blob/master/docs/users/integration.md

target_link_libraries(Engine PRIVATE
    glfw
    imgui::imgui
    Microsoft::DirectXTK12
    Microsoft::DirectX-Headers
    d3d12
    dxgi
    glog::glog
    yaml-cpp::yaml-cpp
    d3dcompiler
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Engine PROPERTY CXX_STANDARD 23)
endif()

# Collect all files in config/ directory
file(GLOB CONFIG_FILES "${CMAKE_SOURCE_DIR}/config/*")

add_custom_command(
    TARGET Engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Engine>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CONFIG_FILES}
        $<TARGET_FILE_DIR:Engine>/config/
    COMMENT "Copying config files to output directory if newer"
)
# TODO: Add tests and install targets if needed.

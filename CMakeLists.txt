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

# Enable testing
enable_testing()

# Default USE_D3D12 to ON on Windows, OFF on other platforms. This allows
# configuring the project to avoid pulling DirectX/D3D12 code on macOS.
if(WIN32)
    set(DEFAULT_USE_D3D12 ON)
else()
    set(DEFAULT_USE_D3D12 OFF)
endif()
option(USE_D3D12 "Enable D3D12 renderer and DirectX dependencies" ${DEFAULT_USE_D3D12})
message(STATUS "USE_D3D12=${USE_D3D12}")

# Find packages via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(unofficial-concurrentqueue CONFIG REQUIRED)

# DirectX / D3D12 related packages are only needed when USE_D3D12 is enabled.
if(USE_D3D12)
    if(NOT WIN32)
        message(FATAL_ERROR "USE_D3D12 is enabled but DirectX is only available on Windows")
    endif()
    find_package(directxtk12 CONFIG REQUIRED)
    find_package(directx-headers CONFIG REQUIRED)
endif()

# TinyGLTF is header-only and doesn't provide CMake config
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

# Automatically add all .cpp and .hpp files in the project directory
file(GLOB ENGINE_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

# If D3D12 support is disabled, filter out any d3d12_* source/header files so
# they are not compiled on non-Windows platforms.
if(NOT USE_D3D12)
    set(_FILTERED_ENGINE_SOURCES "")
    foreach(_src IN LISTS ENGINE_SOURCES)
        get_filename_component(_fname ${_src} NAME)
        if(NOT "${_fname}" MATCHES "^d3d12_")
            list(APPEND _FILTERED_ENGINE_SOURCES ${_src})
        else()
            message(STATUS "Excluding D3D12 source from build: ${_fname}")
        endif()
    endforeach()
    set(ENGINE_SOURCES ${_FILTERED_ENGINE_SOURCES})
endif()

#==============================================================================
# HLSL Shader Compilation System
#==============================================================================

# Find all HLSL shader files in multiple directories
file(GLOB_RECURSE HLSL_FILES 
    "${CMAKE_SOURCE_DIR}/shaders/*.hlsl"
)

# Helper function to compile HLSL shaders
# Parameters:
#   TARGET_NAME - The CMake target to associate the shader with
#   HLSL_FILE   - Path to the .hlsl source file
#   SHADER_TYPE - Type of shader (vs, ps, cs, gs, hs, ds)
#   ENTRY_POINT - Name of the entry point function in the shader
function(add_hlsl_shader TARGET_NAME HLSL_FILE SHADER_TYPE ENTRY_POINT)
    get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
    get_filename_component(SHADER_DIR ${HLSL_FILE} DIRECTORY)
    get_filename_component(SHADER_DIR_NAME ${SHADER_DIR} NAME)
    
    # Create a unique output name that includes the source directory
    # For custom entry points (not standard ones), include entry point name
    set(STANDARD_ENTRY_POINTS "VSMain;PSMain;CSMain;GSMain;HSMain;DSMain")
    if(ENTRY_POINT IN_LIST STANDARD_ENTRY_POINTS)
        set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_${SHADER_TYPE}.cso")
    else()
        set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_${SHADER_TYPE}_${ENTRY_POINT}.cso")
    endif()
    
    # Ensure output directory exists
    get_filename_component(COMPILED_SHADER_DIR ${COMPILED_SHADER} DIRECTORY)
    file(MAKE_DIRECTORY ${COMPILED_SHADER_DIR})
    
    # Find fxc.exe compiler (Windows + D3D12 only)
    if(WIN32 AND USE_D3D12)
        find_program(FXC_EXECUTABLE
            NAMES fxc.exe
            PATHS
                # Try Windows SDK paths
                "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x64"
                "$ENV{WindowsSdkDir}bin/$ENV{WindowsSDKVersion}x86"
                "$ENV{WindowsSdkDir}bin/x64"
                "$ENV{WindowsSdkDir}bin/x86"
                "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
                "C:/Program Files (x86)/Windows Kits/10/bin/*/x86"
                "C:/Program Files (x86)/Windows Kits/8.1/bin/x64"
                "C:/Program Files (x86)/Windows Kits/8.1/bin/x86"
            DOC "DirectX Shader Compiler (fxc.exe)"
        )
        
        if(NOT FXC_EXECUTABLE)
            message(WARNING "fxc.exe not found. HLSL shaders will not be compiled.")
            return()
        endif()
        
        # Determine shader model (using 5.1 for D3D12 compatibility)
        set(SHADER_MODEL "${SHADER_TYPE}_5_1")
        
        # Add custom command to compile the shader
        add_custom_command(
            OUTPUT ${COMPILED_SHADER}
            COMMAND ${FXC_EXECUTABLE} 
                /T ${SHADER_MODEL}          # Target shader model
                /E ${ENTRY_POINT}           # Entry point function name
                /Fo ${COMPILED_SHADER}      # Output file
                ${HLSL_FILE}                # Input source file
            DEPENDS ${HLSL_FILE}
            COMMENT "Compiling HLSL shader: ${SHADER_DIR_NAME}/${SHADER_NAME} (${SHADER_TYPE})"
            VERBATIM
        )
        
        # Add to target sources so CMake tracks dependencies properly
        target_sources(${TARGET_NAME} PRIVATE ${COMPILED_SHADER})
        set_source_files_properties(${COMPILED_SHADER} PROPERTIES GENERATED TRUE)
    endif()
endfunction()

# Define common entry points and their associated shader types  
# Format: "EntryPointName:ShaderType"
set(SHADER_ENTRY_POINTS
    "VSMain:vs"    # Vertex Shader
    "PSMain:ps"    # Pixel Shader
    "CSMain:cs"    # Compute Shader
    "GSMain:gs"    # Geometry Shader
    "HSMain:hs"    # Hull Shader (Tessellation)
    "DSMain:ds"    # Domain Shader (Tessellation)
)

#==============================================================================
# Main Engine Executable
#==============================================================================

add_executable (Engine ${ENGINE_SOURCES} ${IMGUI_IMPL_SOURCES})

if(USE_D3D12)

# Process all HLSL files and compile shaders
set(ALL_COMPILED_SHADERS "")

foreach(HLSL_FILE ${HLSL_FILES})
    get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
    get_filename_component(SHADER_DIR ${HLSL_FILE} DIRECTORY)
    get_filename_component(SHADER_DIR_NAME ${SHADER_DIR} NAME)
    
    message(STATUS "Processing HLSL file: ${HLSL_FILE}")
    
    # Read shader content to automatically detect entry points
    file(READ ${HLSL_FILE} SHADER_CONTENT)
    
    # Check for each type of entry point in the shader source
    foreach(ENTRY_POINT_PAIR ${SHADER_ENTRY_POINTS})
        string(REPLACE ":" ";" ENTRY_PARTS ${ENTRY_POINT_PAIR})
        list(GET ENTRY_PARTS 0 ENTRY_POINT)
        list(GET ENTRY_PARTS 1 SHADER_TYPE)
        
        # If the entry point function is found in the shader source
        if("${SHADER_CONTENT}" MATCHES "${ENTRY_POINT}")
            add_hlsl_shader(Engine ${HLSL_FILE} ${SHADER_TYPE} ${ENTRY_POINT})
            # Use the same path format as the add_hlsl_shader function
            set(COMPILED_SHADER_PATH "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_${SHADER_TYPE}.cso")
            list(APPEND ALL_COMPILED_SHADERS ${COMPILED_SHADER_PATH})
            message(STATUS "  Found ${ENTRY_POINT} -> will compile as ${SHADER_TYPE}")
        endif()
    endforeach()
    
    # Additionally, detect compute shaders with custom entry points by looking for [numthreads(...)] attribute
    string(REGEX MATCHALL "\\[numthreads\\([^)]+\\)\\][^{]*void[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_]*)" COMPUTE_MATCHES "${SHADER_CONTENT}")
    foreach(COMPUTE_MATCH ${COMPUTE_MATCHES})
        # Extract function name from the match
        string(REGEX REPLACE ".*void[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_]*)" "\\1" COMPUTE_ENTRY_POINT "${COMPUTE_MATCH}")
        
        # Check if this entry point was already processed by the standard entry points
        set(ALREADY_PROCESSED FALSE)
        foreach(ENTRY_POINT_PAIR ${SHADER_ENTRY_POINTS})
            string(REPLACE ":" ";" ENTRY_PARTS ${ENTRY_POINT_PAIR})
            list(GET ENTRY_PARTS 0 STANDARD_ENTRY_POINT)
            if("${COMPUTE_ENTRY_POINT}" STREQUAL "${STANDARD_ENTRY_POINT}")
                set(ALREADY_PROCESSED TRUE)
                break()
            endif()
        endforeach()
        
        # If not already processed, add it as a compute shader
        if(NOT ALREADY_PROCESSED)
            add_hlsl_shader(Engine ${HLSL_FILE} cs ${COMPUTE_ENTRY_POINT})
            set(COMPILED_SHADER_PATH "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}_cs_${COMPUTE_ENTRY_POINT}.cso")
            list(APPEND ALL_COMPILED_SHADERS ${COMPILED_SHADER_PATH})
            message(STATUS "  Found compute shader ${COMPUTE_ENTRY_POINT} -> will compile as cs")
        endif()
    endforeach()
endforeach()

# Create a custom target that depends on all compiled shaders
if(ALL_COMPILED_SHADERS)
    list(LENGTH ALL_COMPILED_SHADERS SHADER_COUNT)
    add_custom_target(CompileShaders ALL DEPENDS ${ALL_COMPILED_SHADERS})
    add_dependencies(Engine CompileShaders)
    message(STATUS "Created CompileShaders target with ${SHADER_COUNT} shaders")
    message(STATUS "Compiled shaders will be available in: ${CMAKE_BINARY_DIR}/shaders/")
endif()

endif() # USE_D3D12

# Link libraries for main engine
target_link_libraries(Engine PRIVATE
        glfw
        imgui::imgui
        glog::glog
        yaml-cpp::yaml-cpp
)

# Link Windows-only DirectX libraries when building on Windows
if(WIN32)
    target_link_libraries(Engine PRIVATE
        Microsoft::DirectXTK12
        Microsoft::DirectX-Headers
        d3d12
        dxgi
        dxguid
    )
endif()

# Add TinyGLTF include directory
target_include_directories(Engine PRIVATE ${TINYGLTF_INCLUDE_DIRS})
if(USE_D3D12)
    target_link_libraries(Engine PRIVATE
        Microsoft::DirectXTK12
        Microsoft::DirectX-Headers
        d3d12
        dxgi
        dxguid
    )
    target_compile_definitions(Engine PRIVATE USE_D3D12=1)
endif()
target_compile_definitions(Engine PRIVATE NOMINMAX)

#==============================================================================
# Test Executable
#==============================================================================

# Create a library with engine sources (excluding main.cpp)
file(GLOB ENGINE_LIB_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")
list(REMOVE_ITEM ENGINE_LIB_SOURCES "${CMAKE_SOURCE_DIR}/main.cpp")

# Filter library sources when D3D12 support is disabled
if(NOT USE_D3D12)
    set(_FILTERED_LIB_SOURCES "")
    foreach(_src IN LISTS ENGINE_LIB_SOURCES)
        get_filename_component(_fname ${_src} NAME)
        if(NOT "${_fname}" MATCHES "^d3d12_")
            list(APPEND _FILTERED_LIB_SOURCES ${_src})
        else()
            message(STATUS "Excluding D3D12 lib source from build: ${_fname}")
        endif()
    endforeach()
    set(ENGINE_LIB_SOURCES ${_FILTERED_LIB_SOURCES})
endif()

add_library(EngineLib STATIC ${ENGINE_LIB_SOURCES})

# Link libraries for engine library
target_link_libraries(EngineLib PUBLIC
        glfw
        imgui::imgui
        glog::glog
        yaml-cpp::yaml-cpp
)

if(WIN32)
    if(USE_D3D12)
        target_link_libraries(EngineLib PUBLIC
            Microsoft::DirectXTK12
            Microsoft::DirectX-Headers
            d3d12
            dxgi
            dxguid
        )
    endif()
endif()

# Set include directories for the library
target_include_directories(EngineLib PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories(EngineLib PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_compile_definitions(EngineLib PRIVATE NOMINMAX)
if(USE_D3D12)
    target_compile_definitions(EngineLib PRIVATE USE_D3D12=1)
endif()

# Find all test source files
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")

# If D3D12 is disabled, exclude tests that target D3D12-specific functionality
if(NOT USE_D3D12)
    set(_FILTERED_TEST_SOURCES "")
    foreach(_ts IN LISTS TEST_SOURCES)
        get_filename_component(_tname ${_ts} NAME)
        if(NOT "${_tname}" MATCHES "^d3d12_")
            list(APPEND _FILTERED_TEST_SOURCES ${_ts})
        else()
            message(STATUS "Excluding D3D12 test from build: ${_tname}")
        endif()
    endforeach()
    set(TEST_SOURCES ${_FILTERED_TEST_SOURCES})
endif()

# Create test executable
add_executable(EngineTests ${TEST_SOURCES})

# Link test executable with engine library and gtest
target_link_libraries(EngineTests PRIVATE 
    EngineLib
    GTest::gtest
    GTest::gtest_main
)

# Set C++ standard for both targets
if (CMAKE_VERSION VERSION_GREATER 3.12)
    set_property(TARGET Engine PROPERTY CXX_STANDARD 23)
    set_property(TARGET EngineLib PROPERTY CXX_STANDARD 23)
    set_property(TARGET EngineTests PROPERTY CXX_STANDARD 23)
endif()

# Register tests with CTest
add_test(NAME EngineUnitTests COMMAND EngineTests)

# Set test properties
set_tests_properties(EngineUnitTests PROPERTIES
    TIMEOUT 300  # 5 minutes timeout
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

#==============================================================================
# Post-build Steps
#==============================================================================

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

if(USE_D3D12 AND ALL_COMPILED_SHADERS)
    add_custom_command(
        TARGET Engine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:Engine>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_BINARY_DIR}/shaders
            $<TARGET_FILE_DIR:Engine>/shaders
        COMMENT "Copying compiled shaders to output directory"
        DEPENDS CompileShaders
    )
endif()

# Copy config files for tests as well
add_custom_command(
    TARGET EngineTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:EngineTests>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CONFIG_FILES}
        $<TARGET_FILE_DIR:EngineTests>/config/
    COMMENT "Copying config files to test output directory if newer"
)

#==============================================================================
# Custom Targets for Testing
#==============================================================================

# Add custom target to run tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS EngineTests
    COMMENT "Running all tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add custom target to run tests with specific filters
add_custom_target(run_world_tests
    COMMAND EngineTests --gtest_filter="WorldTest*"
    DEPENDS EngineTests
    COMMENT "Running World tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(run_engine_tests
    COMMAND EngineTests --gtest_filter="EngineTest*"
    DEPENDS EngineTests
    COMMENT "Running Engine tests only"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Add custom target for performance tests
add_custom_target(run_performance_tests
    COMMAND EngineTests --gtest_filter="PerformanceTest*"
    DEPENDS EngineTests
    COMMENT "Running performance tests"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

message(STATUS "Test targets created:")
message(STATUS "  - run_tests: Run all tests")
message(STATUS "  - run_world_tests: Run World class tests only")
message(STATUS "  - run_engine_tests: Run Engine class tests only") 
message(STATUS "  - run_performance_tests: Run performance tests only")
message(STATUS "Use 'cmake --build . --target <target_name>' to run specific test suites")

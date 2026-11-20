# Compatibility builds translate the same SPIR-V as Vulkan. No compiler DLL is needed at runtime.
include(FetchContent)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_ENABLE_C_API OFF)
set(SPIRV_CROSS_ENABLE_HLSL OFF)
set(SPIRV_CROSS_ENABLE_MSL OFF)
set(SPIRV_CROSS_ENABLE_CPP OFF)
set(SPIRV_CROSS_ENABLE_REFLECT OFF)
set(SPIRV_CROSS_SKIP_INSTALL ON)
FetchContent_Declare(glw_spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG 7affe74d77f93a622bb5002789d5332d32e512ee)
# Only SPIRV-Cross targets are created in this directory. OpenGL.cpp belongs to GLWidgets.
if(GLW_WARNINGS_AS_ERRORS)
    set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
endif()
FetchContent_MakeAvailable(glw_spirv_cross)
target_link_libraries(GLWidgets PRIVATE spirv-cross-glsl)
target_compile_definitions(GLWidgets PRIVATE GLW_OPENGL_FALLBACK=1)
target_sources(GLWidgets PRIVATE OpenGL.cpp)

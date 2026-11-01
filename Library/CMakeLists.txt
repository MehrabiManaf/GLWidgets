# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
add_library(GLWidgets ${GLW_LIB_TYPE}
    Source/Core/Assert.cpp
    Source/Core/Error.cpp
    Source/Paint/Painter.cpp
    Source/Paint/Renderer.cpp
    Source/Platform/Platform.cpp
    Source/Platform/Sdl/SdlKeys.cpp
    Source/Platform/Sdl/SdlPlatform.cpp
    Source/Platform/Sdl/SdlWindow.cpp
    Source/Rhi/Device.cpp
    Source/Rhi/SdlGpu/SdlGpuDevice.cpp)
add_library(GLWidgets::GLWidgets ALIAS GLWidgets)
set_target_properties(GLWidgets PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    CXX_VISIBILITY_PRESET hidden            # GCC / Clang: only what GLW_EXPORT marks is exported, as on Windows
    VISIBILITY_INLINES_HIDDEN ON)
# GLW_EXPORT: <build>/Library/Include/GLWidgets/Export.hpp
generate_export_header(GLWidgets
    BASE_NAME GLW
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/Include/GLWidgets/Export.hpp)
if(NOT GLW_SHARED)
    target_compile_definitions(GLWidgets PUBLIC GLW_STATIC_DEFINE)
endif()
target_include_directories(GLWidgets
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/Include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        Source)
target_compile_features(GLWidgets PUBLIC cxx_std_20)
target_link_libraries(GLWidgets PRIVATE ${GLW_SDL3_TARGET})

# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT

# ---- the default font, compiled in ----
set(GLW_DEFAULT_FONT ${CMAKE_CURRENT_SOURCE_DIR}/Fonts/Vazirmatn-Regular.ttf)
set(GLW_DEFAULT_FONT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/Generated/DefaultFont.cpp)
add_custom_command(
    OUTPUT ${GLW_DEFAULT_FONT_SOURCE}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${GLW_DEFAULT_FONT} -DOUTPUT=${GLW_DEFAULT_FONT_SOURCE}
            -DNAMESPACE=glw::text::detail -DNAME=defaultFont -P ${PROJECT_SOURCE_DIR}/cmake/EmbedFile.cmake
    DEPENDS ${GLW_DEFAULT_FONT} ${PROJECT_SOURCE_DIR}/cmake/EmbedFile.cmake
    COMMENT "Embedding the default font"
    VERBATIM)

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
    Source/Rhi/SdlGpu/SdlGpuDevice.cpp
    Source/Text/DrawText.cpp
    Source/Text/Font.cpp
    Source/Text/GlyphAtlas.cpp
    Source/Text/TextLayout.cpp
    Source/Widgets/Application.cpp
    Source/Widgets/Button.cpp
    Source/Widgets/CheckBox.cpp
    Source/Widgets/Docking.cpp
    Source/Widgets/ItemModel.cpp
    Source/Widgets/Json.cpp
    Source/Widgets/Label.cpp
    Source/Widgets/Layout.cpp
    Source/Widgets/LineEdit.cpp
    Source/Widgets/ListView.cpp
    Source/Widgets/Localization.cpp
    Source/Widgets/MajorTabs.cpp
    Source/Widgets/Panel.cpp
    Source/Widgets/RowHeights.cpp
    Source/Widgets/ScrollArea.cpp
    Source/Widgets/Splitter.cpp
    Source/Widgets/Theme.cpp
    Source/Widgets/TreeView.cpp
    Source/Widgets/UiRoot.cpp
    Source/Widgets/Widget.cpp
    Source/Widgets/WindowFrame.cpp
    Source/Widgets/Workspace.cpp
    ${GLW_DEFAULT_FONT_SOURCE})
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
target_link_libraries(GLWidgets PRIVATE ${GLW_SDL3_TARGET} glw_freetype glw_harfbuzz glw_sheenbidi glw_unibreak)

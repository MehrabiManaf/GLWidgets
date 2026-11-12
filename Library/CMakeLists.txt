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
    Source/Charts/BarChart.cpp
    Source/Charts/BarRace.cpp
    Source/Charts/BoxChart.cpp
    Source/Charts/CandleChart.cpp
    Source/Charts/Chart.cpp
    Source/Charts/ChartAxis.cpp
    Source/Charts/FlameGraph.cpp
    Source/Charts/Gauge.cpp
    Source/Charts/HeatMap.cpp
    Source/Charts/HistogramChart.cpp
    Source/Charts/LineChart.cpp
    Source/Charts/PieChart.cpp
    Source/Charts/SankeyChart.cpp
    Source/Charts/ScatterChart.cpp
    Source/Charts/SeriesBuffer.cpp
    Source/Charts/Sparkline.cpp
    Source/Charts/TimelineChart.cpp
    Source/Charts/TreeMap.cpp
    Source/Core/Assert.cpp
    Source/Core/Error.cpp
    Source/Paint/Painter.cpp
    Source/Paint/PainterScene.cpp
    Source/Paint/Renderer.cpp
    Source/Platform/Platform.cpp
    Source/Platform/Sdl/SdlKeys.cpp
    Source/Platform/Sdl/SdlPlatform.cpp
    Source/Platform/Sdl/SdlWindow.cpp
    Source/Rhi/Device.cpp
    Source/Rhi/SdlGpu/SdlGpuDevice.cpp
    Source/Text/DrawText.cpp
    Source/Text/DistanceField.cpp
    Source/Text/Font.cpp
    Source/Text/SystemFonts.cpp
    Source/Text/GlyphAtlas.cpp
    Source/Text/TextLayout.cpp
    Source/Widgets/Action.cpp
    Source/Widgets/Animation.cpp
    Source/Widgets/Application.cpp
    Source/Widgets/Bevel.cpp
    Source/Widgets/Button.cpp
    Source/Widgets/Canvas.cpp
    Source/Widgets/CheckBox.cpp
    Source/Widgets/CollapsiblePanel.cpp
    Source/Widgets/ComboBox.cpp
    Source/Widgets/Dialog.cpp
    Source/Widgets/Docking.cpp
    Source/Widgets/Highlighter.cpp
    Source/Widgets/Icon.cpp
    Source/Widgets/ImageStore.cpp
    Source/Widgets/ItemEditor.cpp
    Source/Widgets/ItemModel.cpp
    Source/Widgets/IndexSet.cpp
    Source/Widgets/ItemOrder.cpp
    Source/Widgets/Json.cpp
    Source/Widgets/Label.cpp
    Source/Widgets/Layout.cpp
    Source/Widgets/LineEdit.cpp
    Source/Widgets/ListView.cpp
    Source/Widgets/Locale.cpp
    Source/Widgets/Menu.cpp
    Source/Widgets/MenuBar.cpp
    Source/Widgets/Localization.cpp
    Source/Widgets/MajorTabs.cpp
    Source/Widgets/Panel.cpp
    Source/Widgets/RowHeights.cpp
    Source/Widgets/ScrollArea.cpp
    Source/Widgets/Settings.cpp
    Source/Widgets/ScrollBar.cpp
    Source/Widgets/SearchPopup.cpp
    Source/Widgets/SpinBox.cpp
    Source/Widgets/Splitter.cpp
    Source/Widgets/StatusBar.cpp
    Source/Widgets/SolidStyle.cpp
    Source/Widgets/Theme.cpp
    Source/Widgets/ToolBar.cpp
    Source/Widgets/TableView.cpp
    Source/Widgets/SortPaint.cpp
    Source/Widgets/TabPaint.cpp
    Source/Widgets/TabView.cpp
    Source/Widgets/TextEdit.cpp
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
target_link_libraries(GLWidgets PRIVATE ${GLW_SDL3_TARGET} glw_freetype glw_harfbuzz glw_sheenbidi glw_unibreak glw_thorvg)
if(WIN32)
    target_link_libraries(GLWidgets PRIVATE dwmapi comctl32)    # rounded windows and shadows; the frame's messages
    target_link_libraries(GLWidgets PRIVATE dwrite)     # the system's fonts for what the text's do not have
elseif(APPLE)
    target_link_libraries(GLWidgets PRIVATE "-framework CoreText" "-framework CoreFoundation")
else()
    # Wayland's drag-and-drop between the application's windows (docking across them), when wayland-client is there
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(GLW_WAYLAND_CLIENT IMPORTED_TARGET wayland-client)
    endif()
    if(GLW_WAYLAND_CLIENT_FOUND)
        target_sources(GLWidgets PRIVATE Source/Platform/Sdl/WaylandDrag.cpp)
        target_link_libraries(GLWidgets PRIVATE PkgConfig::GLW_WAYLAND_CLIENT)
        target_compile_definitions(GLWidgets PRIVATE GLW_HAVE_WAYLAND)
    else()
        message(STATUS "GLWidgets: wayland-client not found: no docking across windows by Wayland's drag-and-drop")
    endif()
    # the system's fonts for what the text's do not have: fontconfig, when it is there (none: no fallback)
    find_package(Fontconfig)
    if(Fontconfig_FOUND)
        target_link_libraries(GLWidgets PRIVATE Fontconfig::Fontconfig)
        target_compile_definitions(GLWidgets PRIVATE GLW_HAVE_FONTCONFIG)
    else()
        message(STATUS "GLWidgets: fontconfig not found: no fallback to the system's fonts")
    endif()
endif()

# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# Tests of what runs without a window or a GPU.
add_executable(glw_tests Main.cpp AnimationTests.cpp BarsTests.cpp CanvasTests.cpp ChartTests.cpp CollapsiblePanelTests.cpp ContainerTests.cpp DeclarativeTests.cpp DockingAcrossWindowsTests.cpp DockingTests.cpp DragDropTests.cpp GridLayoutTests.cpp ItemEditingTests.cpp ItemViewTests.cpp LineEditTests.cpp LocaleTests.cpp LocalizationTests.cpp MenuTests.cpp SceneTests.cpp ScrollBarTests.cpp SelectionTests.cpp SearchFieldTests.cpp SettingsTests.cpp SpinBoxTests.cpp TabViewTests.cpp StyleTests.cpp TableViewTests.cpp TextEditTests.cpp TextLayoutTests.cpp TreeViewTests.cpp WidgetTests.cpp)
target_link_libraries(glw_tests PRIVATE GLWidgets::GLWidgets)
add_test(NAME glw_tests COMMAND glw_tests)

# The tool that brings translation catalogs up to date (Tools/UpdateCatalog.py), where there is a Python to run it.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME update_catalog COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/UpdateCatalogTests.py)
endif()

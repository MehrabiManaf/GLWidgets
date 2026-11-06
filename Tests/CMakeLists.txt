# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# Tests of what runs without a window or a GPU.
add_executable(glw_tests Main.cpp AnimationTests.cpp BarsTests.cpp CanvasTests.cpp ChartTests.cpp CollapsiblePanelTests.cpp ContainerTests.cpp DeclarativeTests.cpp DockingAcrossWindowsTests.cpp DockingTests.cpp ItemEditingTests.cpp ItemViewTests.cpp LineEditTests.cpp LocalizationTests.cpp MenuTests.cpp ScrollBarTests.cpp SelectionTests.cpp StyleTests.cpp TableViewTests.cpp TextEditTests.cpp TextLayoutTests.cpp TreeViewTests.cpp WidgetTests.cpp)
target_link_libraries(glw_tests PRIVATE GLWidgets::GLWidgets)
add_test(NAME glw_tests COMMAND glw_tests)

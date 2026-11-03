# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# How long the widget layer takes, with many widgets: run by hand (a Release build), compared before and after a change.
add_executable(glw_benchmarks Main.cpp)
target_link_libraries(glw_benchmarks PRIVATE GLWidgets::GLWidgets)

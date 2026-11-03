# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# 3.24: what SDL3 needs on macOS ($<LINK_LIBRARY:...>)
cmake_minimum_required(VERSION 3.24)

project(GLWidgets VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)     # SDL3 is linked statically into a shared GLWidgets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(GLW_SHARED          "Build GLWidgets as a shared library"                  ON)
option(GLW_BUILD_EXAMPLES  "Build the examples"                                   ON)
option(GLW_BUILD_TESTS     "Build the tests"                                      ON)
option(GLW_BUILD_BENCHMARKS "Build the benchmarks"                                ON)
option(GLW_USE_SYSTEM_SDL3 "Use an installed SDL3 (find_package) instead of the pinned release" OFF)

if(GLW_SHARED)
    set(GLW_LIB_TYPE SHARED)
else()
    set(GLW_LIB_TYPE STATIC)
endif()
# The library exports its public API only: GLW_EXPORT (generated GLWidgets/Export.hpp) marks it, everything else stays inside
# (hidden visibility on GCC / Clang as well). SDL3 is an implementation detail: none of its types are in a public header.
include(GenerateExportHeader)

# ---- third party: before the warning options below, which are for our code only ----
include(cmake/Dependencies.cmake)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /wd4100 /wd4127 /wd4251 /wd4275)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

add_subdirectory(Library)

if(GLW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(GLW_BUILD_EXAMPLES)
    add_subdirectory(Examples)
endif()

if(GLW_BUILD_BENCHMARKS)
    add_subdirectory(Benchmarks)
endif()

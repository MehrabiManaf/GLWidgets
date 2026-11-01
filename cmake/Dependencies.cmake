# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# The third party libraries. Every one has a permissive license (Docs/Architecture.md, "Dependencies"), is pinned to a release
# and checked against its SHA-256. All are linked into GLWidgets privately: none of their headers is in a public header.
#   GLW_SDL3_TARGET   SDL3
#   glw_freetype      FreeType       glyph rasterization
#   glw_harfbuzz      HarfBuzz       shaping
#   glw_sheenbidi     SheenBidi      the bidirectional algorithm, script runs
#   glw_unibreak      libunibreak    line and grapheme breaks
include(FetchContent)

# ---- SDL3 (zlib): windows, events, input, clipboard, GPU (Vulkan / Direct3D 12 / Metal) ----
if(GLW_USE_SYSTEM_SDL3)
    find_package(SDL3 3.4 CONFIG REQUIRED)
    set(GLW_SDL3_TARGET SDL3::SDL3)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    # the subsystems a GUI does not use; video, GPU, events, dialogs, tray and power stay
    foreach(subsystem AUDIO RENDER CAMERA JOYSTICK HAPTIC HIDAPI SENSOR)
        set(SDL_${subsystem} OFF CACHE BOOL "" FORCE)
    endforeach()
    FetchContent_Declare(SDL3
        URL      https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9C75CF16330322C217DEDD2E0609F1124F1B54B8633E763467B4684D0F4334A3
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(SDL3)
    set(GLW_SDL3_TARGET SDL3::SDL3-static)
endif()

# ---- FreeType 2.15.0 (FTL): glyph rasterization; without the optional libraries (zlib, PNG...) of the system ----
foreach(optional ZLIB BZIP2 PNG HARFBUZZ BROTLI)
    set(FT_DISABLE_${optional} ON CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(freetype
    URL      https://github.com/freetype/freetype/archive/refs/tags/VER-2-15-0.tar.gz
    URL_HASH SHA256=87C7F1F8699B91E89F773F7DCFA505B044E1DAE67F62A55D4E29C3AFDCAC4FB8
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(freetype)
add_library(glw_freetype INTERFACE)
target_link_libraries(glw_freetype INTERFACE freetype)

# ---- HarfBuzz 14.6.0 (MIT): its single-file build, with its own OpenType functions (FreeType is not needed) ----
FetchContent_Declare(harfbuzz
    URL      https://github.com/harfbuzz/harfbuzz/releases/download/14.6.0/harfbuzz-14.6.0.tar.xz
    URL_HASH SHA256=D07A007327277708A2A73AE437887CDBAF282937F6D03CA5467723E9099AF586
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_SUBDIR none)     # only the sources: built here, not by its own (deprecated) CMake build
FetchContent_MakeAvailable(harfbuzz)
add_library(glw_harfbuzz STATIC ${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc)
target_include_directories(glw_harfbuzz SYSTEM PUBLIC ${harfbuzz_SOURCE_DIR}/src)
target_compile_features(glw_harfbuzz PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(glw_harfbuzz PRIVATE /bigobj /utf-8)
endif()

# ---- SheenBidi 3.0.0 (Apache-2.0): the Unicode bidirectional algorithm, script runs ----
set(SB_CONFIG_UNITY ON CACHE BOOL "" FORCE)
FetchContent_Declare(SheenBidi
    URL      https://github.com/Tehreer/SheenBidi/archive/refs/tags/v3.0.0.tar.gz
    URL_HASH SHA256=86C56014034739BA39A24C23EB00323B0BF6F737354F665786015FCA842AF786
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(SheenBidi)
add_library(glw_sheenbidi INTERFACE)
target_link_libraries(glw_sheenbidi INTERFACE SheenBidi::SheenBidi)

# ---- libunibreak 8.0 (zlib): line breaks (UAX #14) and grapheme clusters (UAX #29) ----
FetchContent_Declare(libunibreak
    URL      https://github.com/adah1972/libunibreak/releases/download/libunibreak_8_0/libunibreak-8.0.tar.gz
    URL_HASH SHA256=9C4FAD6E517338A098373ACC9F35579AE2C325E6446666FB9AC2666BA15CEBA4
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SOURCE_SUBDIR none)     # an autotools project: its sources are built here
FetchContent_MakeAvailable(libunibreak)
add_library(glw_unibreak STATIC)
# its Makefile.am's sources; the *data.c files are included by them
foreach(source unibreakbase unibreakdef linebreak linebreakdata linebreakdef eastasianwidthdef emojidef graphemebreak wordbreak)
    target_sources(glw_unibreak PRIVATE ${libunibreak_SOURCE_DIR}/src/${source}.c)
endforeach()
target_include_directories(glw_unibreak SYSTEM PUBLIC ${libunibreak_SOURCE_DIR}/src)

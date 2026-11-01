# SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
# SPDX-License-Identifier: MIT
# The third party libraries. Every one has a permissive license (Docs/Architecture.md, "Dependencies"), is pinned to a release
# and checked against its SHA-256. GLW_SDL3_TARGET is the target to link SDL3 with.
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

# Copyright 2025-2026 Nuo Shen, Nanjing University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)
project(uemu-ng LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Compiler flags for GCC/Clang
add_compile_options(-Wall -Wextra)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Find required packages
find_package(CLI11 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ---- UI Backend selection ----
set(UEMU_UI_BACKEND "gtk4" CACHE STRING "UI backend: gtk4, sfml3, or headless")
set_property(CACHE UEMU_UI_BACKEND PROPERTY STRINGS "gtk4" "sfml3" "headless")

# SFML3 (optional, for sfml3 backend)
find_package(SFML 3 QUIET COMPONENTS Graphics)

# gtkmm4 + VTE4 (optional, for gtk4 backend)
pkg_check_modules(GTKMM4 IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(VTE4 IMPORTED_TARGET vte-2.91-gtk4)

# Validate backend selection
set(UEMU_UI_DEFINE "")
if(UEMU_UI_BACKEND STREQUAL "gtk4")
    if(NOT GTKMM4_FOUND)
        message(FATAL_ERROR "GTK4 backend requires gtkmm-4.0 (package gtkmm-4.0)")
    endif()
    if(NOT VTE4_FOUND)
        message(FATAL_ERROR "GTK4 backend requires vte-2.91-gtk4 (package vte4)")
    endif()
    set(UEMU_UI_DEFINE UEMU_UI_GTK4)
    message(STATUS "UI backend: GTK4")
elseif(UEMU_UI_BACKEND STREQUAL "sfml3")
    if(NOT SFML_FOUND)
        message(FATAL_ERROR "SFML3 backend requires SFML 3 (package sfml)")
    endif()
    set(UEMU_UI_DEFINE UEMU_UI_SFML3)
    message(STATUS "UI backend: SFML3")
elseif(UEMU_UI_BACKEND STREQUAL "headless")
    set(UEMU_UI_DEFINE UEMU_UI_HEADLESS)
    message(STATUS "UI backend: headless")
else()
    message(FATAL_ERROR "Unknown UEMU_UI_BACKEND=${UEMU_UI_BACKEND}. Valid: gtk4, sfml3, headless")
endif()

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/berkeley-softfloat-3.cmake)

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

add_library(uemu_core STATIC ${SOURCES} ${HEADERS})

target_include_directories(uemu_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/kernel-headers/generic/include
)

target_compile_definitions(uemu_core PRIVATE ${UEMU_UI_DEFINE})

# Link libraries based on backend choice
target_link_libraries(uemu_core
    PUBLIC
        CLI11::CLI11
        Threads::Threads
        softfloat
)

if(UEMU_UI_BACKEND STREQUAL "gtk4")
    target_link_libraries(uemu_core PUBLIC PkgConfig::GTKMM4 PkgConfig::VTE4)
elseif(UEMU_UI_BACKEND STREQUAL "sfml3")
    target_link_libraries(uemu_core PUBLIC SFML::Graphics)
endif()

add_executable(uemu src/main.cpp)
target_link_libraries(uemu PRIVATE uemu_core)
target_include_directories(uemu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/riscv-tests.cmake)

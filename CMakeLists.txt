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
project(uemu-ng LANGUAGES C CXX ASM)

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
find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.14)
pkg_check_modules(ADWAITA REQUIRED IMPORTED_TARGET libadwaita-1>=1.4)
pkg_check_modules(VTE REQUIRED IMPORTED_TARGET vte-2.91-gtk4>=0.68)

# libfdt ships no pkg-config file on Ubuntu/Debian, so find the header and the
# library directly and hand both to the targets through one imported target.
find_path(LIBFDT_INCLUDE_DIR NAMES libfdt.h)
find_library(LIBFDT_LIBRARY NAMES fdt)

if(NOT LIBFDT_INCLUDE_DIR OR NOT LIBFDT_LIBRARY)
    message(FATAL_ERROR "libfdt was not found (libfdt.h and libfdt are both "
                        "needed; install libfdt-dev)")
endif()

add_library(LibFDT::LibFDT UNKNOWN IMPORTED)
set_target_properties(LibFDT::LibFDT PROPERTIES
    IMPORTED_LOCATION "${LIBFDT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFDT_INCLUDE_DIR}"
)

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/berkeley-softfloat-3.cmake)

# Generate .S from .S.in to bake in absolute icon path
set(UOTAN_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets/uotan.png)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/frontend/uotan_icon.S.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/uotan_icon.S
    @ONLY
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# The emulator core: guest execution, memory, devices.  It must stay free of
# any frontend dependency (see tests/architecture/check_layering.cmake).
file(GLOB_RECURSE CORE_SOURCES
    "src/common/*.cpp"
    "src/core/*.cpp"
    "src/device/*.cpp"
    "src/utils/*.cpp"
    "src/emulator.cpp"
    "src/fdt_generator.cpp"
)

add_library(uemu_core STATIC ${CORE_SOURCES} ${HEADERS})

target_include_directories(uemu_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/kernel-headers/generic/include
)

# Link libraries that the core needs.  libfdt is an implementation detail of
# src/utils/fdt.cpp and src/emulator.cpp, so it stays private; the tests link it
# themselves because they use it to inspect the trees they build.
target_link_libraries(uemu_core
    PUBLIC
        Threads::Threads
        softfloat
    PRIVATE
        LibFDT::LibFDT
)

# The host frontend: everything that talks to the user (terminal, window,
# input devices).  It is only linked into the executable, never into tests.
file(GLOB_RECURSE FRONTEND_SOURCES
    "src/frontend/*.cpp"
    "src/frontend/*.S"
)

list(APPEND FRONTEND_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated/uotan_icon.S)

add_library(uemu_frontend STATIC ${FRONTEND_SOURCES})
target_link_libraries(uemu_frontend
    PUBLIC
        uemu_core
        SDL3::SDL3
        SDL3_image::SDL3_image
    PRIVATE
        PkgConfig::GTK4
        PkgConfig::ADWAITA
        PkgConfig::VTE
)

add_executable(uemu src/main.cpp)
target_link_libraries(uemu PRIVATE uemu_frontend CLI11::CLI11)
target_include_directories(uemu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/riscv-tests.cmake)

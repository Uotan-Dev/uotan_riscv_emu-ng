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

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/berkeley-softfloat-3.cmake)

# Generate .S from .S.in to bake in absolute icon path
set(UOTAN_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets/uotan.png)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/uotan_icon.S.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/uotan_icon.S
    @ONLY
)

file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
    "src/*.S"
)

list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated/uotan_icon.S)

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

# Link libraries that the core needs
target_link_libraries(uemu_core
    PUBLIC
        CLI11::CLI11
        Threads::Threads
        softfloat
        SDL3::SDL3
        SDL3_image::SDL3_image
)

add_executable(uemu src/main.cpp)
target_link_libraries(uemu PRIVATE uemu_core)
target_include_directories(uemu PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/thirdparty/riscv-tests.cmake)

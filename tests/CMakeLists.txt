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

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG b2b9072ecbe874f5937054653ef8f2731eb0f010
)

FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE TEST_SOURCES 
    "*.cpp"
)

add_definitions(-DRISCV_TEST_DIR="${CMAKE_CURRENT_BINARY_DIR}")
add_executable(uemu_test ${TEST_SOURCES})

target_link_libraries(uemu_test 
    PRIVATE 
    GTest::gtest_main 
    GTest::gtest
    uemu_core
)

target_include_directories(uemu_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include(GoogleTest)
gtest_discover_tests(uemu_test)

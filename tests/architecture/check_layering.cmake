# Copyright 2026 Nuo Shen, Nanjing University
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
#
# Fails when the emulator core or the device layer reaches into the host
# frontend or touches the host streams directly.  The dependency direction is
# frontend -> core, never the reverse; stdout is reserved for guest-visible
# console output, and diagnostics go through uemu::log (include/common/log.hpp),
# the only place allowed to write to stderr.
# Run by CTest as Architecture.NoFrontendDependency.

if(NOT DEFINED PROJECT_ROOT)
    message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

set(_layers "include/core" "include/device" "src/core" "src/device")

# Core-side files that live outside those directories.
set(_extra_files
    "include/board_config.hpp"
    "include/common/address_range.hpp"
    "include/emulator.hpp"
    "include/fdt_generator.hpp"
    "include/utils/elf_loader.hpp"
    "include/utils/fdt.hpp"
    "src/emulator.cpp"
    "src/fdt_generator.cpp"
    "src/utils/elf_loader.cpp"
    "src/utils/fdt.cpp"
)

# Host-only headers the core must not include.
set(_forbidden_includes
    "#include[ \t]*[<\"]frontend/"
    "#include[ \t]*<SDL"
    "#include[ \t]*<termios\\.h>"
    "#include[ \t]*<unistd\\.h>"
    "#include[ \t]*<fcntl\\.h>"
    "#include[ \t]*<sys/ioctl\\.h>"
)

# Host-only symbols that must not be used, even without an include.  Diagnostics
# belong to uemu::log, not to the host streams.
set(_forbidden_tokens
    "frontend::"
    "ui::"
    "SDL_[A-Za-z]"
    "std::cout"
    "std::cerr"
    "std::ostream"
    "std::print" # also matches std::println
    "stderr"
    # The device tree's "stdout-path" property is guest-visible data, not host
    # output; a hyphen is the only thing allowed to follow the name.
    "stdout([^-]|$)"
)

set(_offenders "")
set(_files "")

foreach(_layer IN LISTS _layers)
    file(GLOB_RECURSE _layer_files
        "${PROJECT_ROOT}/${_layer}/*.h"
        "${PROJECT_ROOT}/${_layer}/*.hpp"
        "${PROJECT_ROOT}/${_layer}/*.cpp"
    )
    list(APPEND _files ${_layer_files})
endforeach()

foreach(_extra IN LISTS _extra_files)
    list(APPEND _files "${PROJECT_ROOT}/${_extra}")
endforeach()

foreach(_file IN LISTS _files)
    file(READ "${_file}" _content)

    foreach(_pattern IN LISTS _forbidden_includes _forbidden_tokens)
        if(_content MATCHES "${_pattern}")
            file(RELATIVE_PATH _relative "${PROJECT_ROOT}" "${_file}")
            list(APPEND _offenders "${_relative} matches '${_pattern}'")
        endif()
    endforeach()
endforeach()

if(_offenders)
    list(JOIN _offenders "\n  " _report)
    message(FATAL_ERROR
        "Disallowed host dependencies found in the emulator core:\n  ${_report}")
endif()

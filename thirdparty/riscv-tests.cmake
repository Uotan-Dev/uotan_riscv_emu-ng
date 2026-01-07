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

find_program(RISCV_GCC NAMES riscv64-unknown-elf-gcc)
find_program(RISCV_OBJCOPY NAMES riscv64-unknown-elf-objcopy)
find_program(RISCV_OBJDUMP NAMES riscv64-unknown-elf-objdump)

set(RISCV_TESTS_DIR ${CMAKE_CURRENT_LIST_DIR}/riscv-tests)
set(RISCV_TEST_ENV_DIR ${CMAKE_CURRENT_LIST_DIR}/riscv-test-env)

# Build function for v-mode tests (virtual memory tests)
function(build_asm_v asm_glob suite_name target_name)
    file(GLOB ASM_FILES ${asm_glob})

    if(NOT ASM_FILES)
        message(WARNING "No assembly files found for pattern: ${asm_glob}")
        return()
    endif()

    set(ELFS)

    foreach(asmfile ${ASM_FILES})
        get_filename_component(fname ${asmfile} NAME_WE)
        # Include suite name in output filename to avoid conflicts
        set(output_elf "${CMAKE_BINARY_DIR}/${suite_name}-${fname}-v.elf")

        # compile .S -> .elf (v-mode)
        add_custom_command(
            OUTPUT ${output_elf}
            COMMAND ${RISCV_GCC}
                -T${RISCV_TEST_ENV_DIR}/v/link.ld
                -I${RISCV_TEST_ENV_DIR}/v
                -I${RISCV_TESTS_DIR}/isa/macros/scalar
                -nostdlib -ffreestanding -march=rv64g -g -mabi=lp64 -nostartfiles
                -mcmodel=medany -fvisibility=hidden -static -std=gnu99 -O2
                -DENTROPY=0xe033f4a
                ${RISCV_TEST_ENV_DIR}/v/entry.S
                ${RISCV_TEST_ENV_DIR}/v/string.c
                ${RISCV_TEST_ENV_DIR}/v/vm.c
                ${asmfile}
                -o ${output_elf}
            DEPENDS ${asmfile} 
                    ${RISCV_TEST_ENV_DIR}/v/link.ld
                    ${RISCV_TEST_ENV_DIR}/v/entry.S
                    ${RISCV_TEST_ENV_DIR}/v/string.c
                    ${RISCV_TEST_ENV_DIR}/v/vm.c
            COMMENT "Compiling ${suite_name}/${fname}.S -> ${output_elf} (v-mode)"
            VERBATIM
        )

        list(APPEND ELFS ${output_elf})
    endforeach()

    add_custom_target(${target_name} ALL
        DEPENDS ${ELFS}
        COMMENT "Building RISC-V v-mode tests: ${target_name}"
    )

    set(${target_name}_ELFS ${ELFS} PARENT_SCOPE)
endfunction()

# Build function for p-mode tests (physical/bare metal tests)
function(build_asm_p asm_glob suite_name target_name)
    file(GLOB ASM_FILES ${asm_glob})

    if(NOT ASM_FILES)
        message(WARNING "No assembly files found for pattern: ${asm_glob}")
        return()
    endif()

    set(ELFS)

    foreach(asmfile ${ASM_FILES})
        get_filename_component(fname ${asmfile} NAME_WE)
        # Include suite name in output filename to avoid conflicts
        set(output_elf "${CMAKE_BINARY_DIR}/${suite_name}-${fname}-p.elf")

        # compile .S -> .elf (p-mode)
        add_custom_command(
            OUTPUT ${output_elf}
            COMMAND ${RISCV_GCC}
                -T${RISCV_TEST_ENV_DIR}/p/link.ld
                -I${RISCV_TEST_ENV_DIR}/p
                -I${RISCV_TESTS_DIR}/isa/macros/scalar
                -nostdlib -ffreestanding -march=rv64g -g -mabi=lp64 -nostartfiles -O2
                ${asmfile}
                -o ${output_elf}
            DEPENDS ${asmfile} ${RISCV_TEST_ENV_DIR}/p/link.ld
            COMMENT "Compiling ${suite_name}/${fname}.S -> ${output_elf} (p-mode)"
            VERBATIM
        )

        list(APPEND ELFS ${output_elf})
    endforeach()

    add_custom_target(${target_name} ALL
        DEPENDS ${ELFS}
        COMMENT "Building RISC-V p-mode tests: ${target_name}"
    )

    set(${target_name}_ELFS ${ELFS} PARENT_SCOPE)
endfunction()

build_asm_p("${RISCV_TESTS_DIR}/isa/rv64mi/*.S" "rv64mi" "riscv_tests_rv64mi_p")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64si/*.S" "rv64si" "riscv_tests_rv64si_p")

build_asm_v("${RISCV_TESTS_DIR}/isa/rv64ui/*.S" "rv64ui" "riscv_tests_rv64ui_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64ui/*.S" "rv64ui" "riscv_tests_rv64ui_p")
build_asm_v("${RISCV_TESTS_DIR}/isa/rv64um/*.S" "rv64um" "riscv_tests_rv64um_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64um/*.S" "rv64um" "riscv_tests_rv64um_p")
build_asm_v("${RISCV_TESTS_DIR}/isa/rv64ua/*.S" "rv64ua" "riscv_tests_rv64ua_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64ua/*.S" "rv64ua" "riscv_tests_rv64ua_p")
build_asm_v("${RISCV_TESTS_DIR}/isa/rv64uf/*.S" "rv64uf" "riscv_tests_rv64uf_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64uf/*.S" "rv64uf" "riscv_tests_rv64uf_p")
build_asm_v("${RISCV_TESTS_DIR}/isa/rv64ud/*.S" "rv64ud" "riscv_tests_rv64ud_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64ud/*.S" "rv64ud" "riscv_tests_rv64ud_p")
build_asm_v("${RISCV_TESTS_DIR}/isa/rv64uc/*.S" "rv64uc" "riscv_tests_rv64uc_v")
build_asm_p("${RISCV_TESTS_DIR}/isa/rv64uc/*.S" "rv64uc" "riscv_tests_rv64uc_p")

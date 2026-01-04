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
set(RISCV_LINKER_SCRIPT ${RISCV_TEST_ENV_DIR}/p/link.ld)

function(build_asm asm_glob out_dir target_name)
    # create output dir
    file(MAKE_DIRECTORY "${out_dir}/bin")
    file(MAKE_DIRECTORY "${out_dir}/dumped")

    file(GLOB ASM_FILES ${asm_glob})

    if(ASM_FILES STREQUAL "ASM_FILES-NOTFOUND")
        message(WARNING "No assembly files found for pattern: ${asm_glob}")
        return()
    endif()

    set(BINARIES)
    set(DUMPS)

    foreach(asmfile ${ASM_FILES})
        get_filename_component(fname ${asmfile} NAME_WE)
        set(temp_elf "${CMAKE_BINARY_DIR}/riscv_test_${target_name}_${fname}.elf")
        set(output_bin "${out_dir}/bin/${fname}.bin")
        set(output_dump "${out_dir}/dumped/${fname}.dump")

        # compile .S -> .elf
        add_custom_command(
            OUTPUT ${temp_elf}
            COMMAND ${RISCV_GCC}
                -T${RISCV_LINKER_SCRIPT}
                -I${RISCV_TEST_ENV_DIR}/p
                -I${RISCV_TESTS_DIR}/isa/macros/scalar
                -nostdlib -ffreestanding -march=rv64imafd_zicsr_zifencei -g -mabi=lp64 -nostartfiles -O0
                -o ${temp_elf}
                ${asmfile}
            DEPENDS ${asmfile} ${RISCV_LINKER_SCRIPT}
            COMMENT "Compiling ${fname}.S -> ${temp_elf}"
            VERBATIM
        )

        # elf -> binary
        add_custom_command(
            OUTPUT ${output_bin}
            COMMAND ${RISCV_OBJCOPY} -O binary ${temp_elf} ${output_bin}
            DEPENDS ${temp_elf}
            COMMENT "objcopy ${temp_elf} -> ${output_bin}"
            VERBATIM
        )

        list(APPEND BINARIES ${output_bin})
        list(APPEND DUMPS ${output_dump})
    endforeach()

    add_custom_target(${target_name} ALL
        DEPENDS ${BINARIES}
        COMMENT "Building RISC-V tests: ${target_name}"
    )

    # Export variables to parent scope
    set(${target_name}_BINARIES ${BINARIES} PARENT_SCOPE)
    set(${target_name}_DUMPS ${DUMPS} PARENT_SCOPE)
endfunction()

# Example: build RV64MI tests (adjust the glob if you want other subdirs)
build_asm("${RISCV_TESTS_DIR}/isa/rv64mi/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64mi" "riscv_tests_rv64mi")
build_asm("${RISCV_TESTS_DIR}/isa/rv64si/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64si" "riscv_tests_rv64si")
build_asm("${RISCV_TESTS_DIR}/isa/rv64ui/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64ui" "riscv_tests_rv64ui")
build_asm("${RISCV_TESTS_DIR}/isa/rv64um/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64um" "riscv_tests_rv64um")
build_asm("${RISCV_TESTS_DIR}/isa/rv64ua/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64ua" "riscv_tests_rv64ua")
build_asm("${RISCV_TESTS_DIR}/isa/rv64uf/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64uf" "riscv_tests_rv64uf")
build_asm("${RISCV_TESTS_DIR}/isa/rv64ud/*.S" "${CMAKE_BINARY_DIR}/testbins/rv64ud" "riscv_tests_rv64ud")

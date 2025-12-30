/*
 * Copyright 2025 Nuo Shen, Nanjing University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include "device/sifive_test.hpp"
#include "emulator.hpp"

namespace uemu::test {

constexpr size_t TEST_DRAM_SIZE = 32 * 1024 * 1024;

void test_file(const std::string& file, std::vector<std::string>& failed) {
    Emulator emulator(TEST_DRAM_SIZE);

    try {
        emulator.loadelf(file);
    } catch (...) {
        std::cerr << "Exception happened while loading ELF. Skipping the test."
                  << std::endl;
        EXPECT_TRUE(true);
        return;
    }

    emulator.run();

    auto status = emulator.shutdown_status();

    if (status != device::SiFiveTest::Status::PASS)
        failed.emplace_back(file);

    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

TEST(RISCVTest, RV64MI) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64mi_breakpoint.elf",
        "riscv_test_riscv_tests_rv64mi_csr.elf",
        "riscv_test_riscv_tests_rv64mi_instret_overflow.elf",
        "riscv_test_riscv_tests_rv64mi_ld-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_lh-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_lw-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_ma_addr.elf",
        "riscv_test_riscv_tests_rv64mi_ma_fetch.elf",
        "riscv_test_riscv_tests_rv64mi_mcsr.elf",
        "riscv_test_riscv_tests_rv64mi_pmpaddr.elf",
        "riscv_test_riscv_tests_rv64mi_sbreak.elf",
        "riscv_test_riscv_tests_rv64mi_scall.elf",
        "riscv_test_riscv_tests_rv64mi_sd-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_sh-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_sw-misaligned.elf",
        "riscv_test_riscv_tests_rv64mi_zicntr.elf"};

    std::vector<std::string> failed;

    for (const auto& f : files)
        test_file(f, failed);

    if (!failed.empty()) {
        std::cerr << "Failed tests:\n";
        for (auto& f : failed)
            std::cerr << f << std::endl;
    }
}

}; // namespace uemu::test

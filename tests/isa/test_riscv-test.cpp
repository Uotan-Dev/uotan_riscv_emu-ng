/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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

#include <filesystem>
#include <gtest/gtest.h>
#include <print>
#include <string>
#include <vector>

#include "device/sifive_test.hpp"
#include "emulator.hpp"

namespace uemu::test {

constexpr size_t TEST_DRAM_SIZE = 32 * 1024 * 1024;

void test_file(const std::string& file, std::vector<std::string>& failed,
               uint16_t expected_status, uint16_t expected_code) {
    Emulator emulator(TEST_DRAM_SIZE);

    try {
        std::filesystem::path test_path = RISCV_TEST_DIR;
        test_path = test_path.parent_path() / file;
        emulator.loadelf(test_path.string());
    } catch (...) {
        std::println(
            stderr, "Exception happened while loading ELF. Skipping the test.");
        EXPECT_TRUE(false);
        return;
    }

    emulator.run();

    auto status = emulator.shutdown_status();

    if (status != device::SiFiveTest::Status::PASS)
        failed.emplace_back(file);

    EXPECT_EQ(emulator.shutdown_status(), expected_status);
    EXPECT_EQ(emulator.shutdown_code(), expected_code);
}

void test_files(const std::vector<std::string>& files,
                uint16_t expected_status = device::SiFiveTest::Status::PASS,
                uint16_t expected_code = 0) {
    std::vector<std::string> failed;

    for (const auto& f : files)
        test_file(f, failed, expected_status, expected_code);

    if (!failed.empty()) {
        std::println(stderr, "Failed tests:");
        for (auto& f : failed)
            std::println(stderr, "{}", f);
    }
}

TEST(RISCVTest, RV64MI_p) {
    std::vector<std::string> files = {
        "rv64mi-breakpoint-p.elf",
        "rv64mi-csr-p.elf",
        "rv64mi-instret_overflow-p.elf",
        "rv64mi-ld-misaligned-p.elf",
        "rv64mi-lh-misaligned-p.elf",
        "rv64mi-lw-misaligned-p.elf",
        "rv64mi-ma_addr-p.elf",
        "rv64mi-ma_fetch-p.elf",
        "rv64mi-mcsr-p.elf",
        "rv64mi-pmpaddr-p.elf",
        "rv64mi-sbreak-p.elf",
        "rv64mi-scall-p.elf",
        "rv64mi-sd-misaligned-p.elf",
        "rv64mi-sh-misaligned-p.elf",
        "rv64mi-sw-misaligned-p.elf",
        "rv64mi-zicntr-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64SI_p) {
    std::vector<std::string> files = {
        "rv64si-csr-p.elf",          "rv64si-dirty-p.elf",
        "rv64si-icache-alias-p.elf", "rv64si-ma_fetch-p.elf",
        "rv64si-sbreak-p.elf",       "rv64si-scall-p.elf",
        "rv64si-wfi-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UI_p) {
    std::vector<std::string> files = {
        "rv64ui-add-p.elf",   "rv64ui-addi-p.elf",    "rv64ui-addiw-p.elf",
        "rv64ui-addw-p.elf",  "rv64ui-and-p.elf",     "rv64ui-andi-p.elf",
        "rv64ui-auipc-p.elf", "rv64ui-beq-p.elf",     "rv64ui-bge-p.elf",
        "rv64ui-bgeu-p.elf",  "rv64ui-blt-p.elf",     "rv64ui-bltu-p.elf",
        "rv64ui-bne-p.elf",   "rv64ui-fence_i-p.elf", "rv64ui-jal-p.elf",
        "rv64ui-jalr-p.elf",  "rv64ui-lb-p.elf",      "rv64ui-lbu-p.elf",
        "rv64ui-ld-p.elf",    "rv64ui-ld_st-p.elf",   "rv64ui-lh-p.elf",
        "rv64ui-lhu-p.elf",   "rv64ui-lui-p.elf",     "rv64ui-lw-p.elf",
        "rv64ui-lwu-p.elf",   "rv64ui-ma_data-p.elf", "rv64ui-or-p.elf",
        "rv64ui-ori-p.elf",   "rv64ui-sb-p.elf",      "rv64ui-sd-p.elf",
        "rv64ui-sh-p.elf",    "rv64ui-simple-p.elf",  "rv64ui-sll-p.elf",
        "rv64ui-slli-p.elf",  "rv64ui-slliw-p.elf",   "rv64ui-sllw-p.elf",
        "rv64ui-slt-p.elf",   "rv64ui-slti-p.elf",    "rv64ui-sltiu-p.elf",
        "rv64ui-sltu-p.elf",  "rv64ui-sra-p.elf",     "rv64ui-srai-p.elf",
        "rv64ui-sraiw-p.elf", "rv64ui-sraw-p.elf",    "rv64ui-srl-p.elf",
        "rv64ui-srli-p.elf",  "rv64ui-srliw-p.elf",   "rv64ui-srlw-p.elf",
        "rv64ui-st_ld-p.elf", "rv64ui-sub-p.elf",     "rv64ui-subw-p.elf",
        "rv64ui-sw-p.elf",    "rv64ui-xor-p.elf",     "rv64ui-xori-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UM_p) {
    std::vector<std::string> files = {
        "rv64um-div-p.elf",    "rv64um-divu-p.elf",  "rv64um-divuw-p.elf",
        "rv64um-divw-p.elf",   "rv64um-mul-p.elf",   "rv64um-mulh-p.elf",
        "rv64um-mulhsu-p.elf", "rv64um-mulhu-p.elf", "rv64um-mulw-p.elf",
        "rv64um-rem-p.elf",    "rv64um-remu-p.elf",  "rv64um-remuw-p.elf",
        "rv64um-remw-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UA_p) {
    std::vector<std::string> files = {
        "rv64ua-amoadd_d-p.elf",  "rv64ua-amoadd_w-p.elf",
        "rv64ua-amoand_d-p.elf",  "rv64ua-amoand_w-p.elf",
        "rv64ua-amomax_d-p.elf",  "rv64ua-amomaxu_d-p.elf",
        "rv64ua-amomaxu_w-p.elf", "rv64ua-amomax_w-p.elf",
        "rv64ua-amomin_d-p.elf",  "rv64ua-amominu_d-p.elf",
        "rv64ua-amominu_w-p.elf", "rv64ua-amomin_w-p.elf",
        "rv64ua-amoor_d-p.elf",   "rv64ua-amoor_w-p.elf",
        "rv64ua-amoswap_d-p.elf", "rv64ua-amoswap_w-p.elf",
        "rv64ua-amoxor_d-p.elf",  "rv64ua-amoxor_w-p.elf",
        "rv64ua-lrsc-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UF_p) {
    std::vector<std::string> files = {
        "rv64uf-fadd-p.elf",  "rv64uf-fclass-p.elf",   "rv64uf-fcmp-p.elf",
        "rv64uf-fcvt-p.elf",  "rv64uf-fcvt_w-p.elf",   "rv64uf-fdiv-p.elf",
        "rv64uf-fmadd-p.elf", "rv64uf-fmin-p.elf",     "rv64uf-ldst-p.elf",
        "rv64uf-move-p.elf",  "rv64uf-recoding-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UD_p) {
    std::vector<std::string> files = {
        "rv64ud-fadd-p.elf",     "rv64ud-fclass-p.elf",
        "rv64ud-fcmp-p.elf",     "rv64ud-fcvt-p.elf",
        "rv64ud-fcvt_w-p.elf",   "rv64ud-fdiv-p.elf",
        "rv64ud-fmadd-p.elf",    "rv64ud-fmin-p.elf",
        "rv64ud-ldst-p.elf",     "rv64ud-move-p.elf",
        "rv64ud-recoding-p.elf", "rv64ud-structural-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UC_p) {
    std::vector<std::string> files = {
        "rv64uc-rvc-p.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UI_v) {
    std::vector<std::string> files = {
        "rv64ui-add-v.elf",   "rv64ui-addi-v.elf",    "rv64ui-addiw-v.elf",
        "rv64ui-addw-v.elf",  "rv64ui-and-v.elf",     "rv64ui-andi-v.elf",
        "rv64ui-auipc-v.elf", "rv64ui-beq-v.elf",     "rv64ui-bge-v.elf",
        "rv64ui-bgeu-v.elf",  "rv64ui-blt-v.elf",     "rv64ui-bltu-v.elf",
        "rv64ui-bne-v.elf",   "rv64ui-fence_i-v.elf", "rv64ui-jal-v.elf",
        "rv64ui-jalr-v.elf",  "rv64ui-lb-v.elf",      "rv64ui-lbu-v.elf",
        "rv64ui-ld-v.elf",    "rv64ui-ld_st-v.elf",   "rv64ui-lh-v.elf",
        "rv64ui-lhu-v.elf",   "rv64ui-lui-v.elf",     "rv64ui-lw-v.elf",
        "rv64ui-lwu-v.elf",   "rv64ui-ma_data-v.elf", "rv64ui-or-v.elf",
        "rv64ui-ori-v.elf",   "rv64ui-sb-v.elf",      "rv64ui-sd-v.elf",
        "rv64ui-sh-v.elf",    "rv64ui-simple-v.elf",  "rv64ui-sll-v.elf",
        "rv64ui-slli-v.elf",  "rv64ui-slliw-v.elf",   "rv64ui-sllw-v.elf",
        "rv64ui-slt-v.elf",   "rv64ui-slti-v.elf",    "rv64ui-sltiu-v.elf",
        "rv64ui-sltu-v.elf",  "rv64ui-sra-v.elf",     "rv64ui-srai-v.elf",
        "rv64ui-sraiw-v.elf", "rv64ui-sraw-v.elf",    "rv64ui-srl-v.elf",
        "rv64ui-srli-v.elf",  "rv64ui-srliw-v.elf",   "rv64ui-srlw-v.elf",
        "rv64ui-st_ld-v.elf", "rv64ui-sub-v.elf",     "rv64ui-subw-v.elf",
        "rv64ui-sw-v.elf",    "rv64ui-xor-v.elf",     "rv64ui-xori-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

TEST(RISCVTest, RV64UM_v) {
    std::vector<std::string> files = {
        "rv64um-div-v.elf",    "rv64um-divu-v.elf",  "rv64um-divuw-v.elf",
        "rv64um-divw-v.elf",   "rv64um-mul-v.elf",   "rv64um-mulh-v.elf",
        "rv64um-mulhsu-v.elf", "rv64um-mulhu-v.elf", "rv64um-mulw-v.elf",
        "rv64um-rem-v.elf",    "rv64um-remu-v.elf",  "rv64um-remuw-v.elf",
        "rv64um-remw-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

TEST(RISCVTest, RV64UA_v) {
    std::vector<std::string> files = {
        "rv64ua-amoadd_d-v.elf",  "rv64ua-amoadd_w-v.elf",
        "rv64ua-amoand_d-v.elf",  "rv64ua-amoand_w-v.elf",
        "rv64ua-amomax_d-v.elf",  "rv64ua-amomaxu_d-v.elf",
        "rv64ua-amomaxu_w-v.elf", "rv64ua-amomax_w-v.elf",
        "rv64ua-amomin_d-v.elf",  "rv64ua-amominu_d-v.elf",
        "rv64ua-amominu_w-v.elf", "rv64ua-amomin_w-v.elf",
        "rv64ua-amoor_d-v.elf",   "rv64ua-amoor_w-v.elf",
        "rv64ua-amoswap_d-v.elf", "rv64ua-amoswap_w-v.elf",
        "rv64ua-amoxor_d-v.elf",  "rv64ua-amoxor_w-v.elf",
        "rv64ua-lrsc-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

TEST(RISCVTest, RV64UF_v) {
    std::vector<std::string> files = {
        "rv64uf-fadd-v.elf",  "rv64uf-fclass-v.elf",   "rv64uf-fcmp-v.elf",
        "rv64uf-fcvt-v.elf",  "rv64uf-fcvt_w-v.elf",   "rv64uf-fdiv-v.elf",
        "rv64uf-fmadd-v.elf", "rv64uf-fmin-v.elf",     "rv64uf-ldst-v.elf",
        "rv64uf-move-v.elf",  "rv64uf-recoding-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

TEST(RISCVTest, RV64UD_v) {
    std::vector<std::string> files = {
        "rv64ud-fadd-v.elf",     "rv64ud-fclass-v.elf",
        "rv64ud-fcmp-v.elf",     "rv64ud-fcvt-v.elf",
        "rv64ud-fcvt_w-v.elf",   "rv64ud-fdiv-v.elf",
        "rv64ud-fmadd-v.elf",    "rv64ud-fmin-v.elf",
        "rv64ud-ldst-v.elf",     "rv64ud-move-v.elf",
        "rv64ud-recoding-v.elf", "rv64ud-structural-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

TEST(RISCVTest, RV64UC_v) {
    std::vector<std::string> files = {
        "rv64uc-rvc-v.elf",
    };

    test_files(files, device::SiFiveTest::Status::PASS, 1);
}

}; // namespace uemu::test

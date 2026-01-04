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
#include <string>
#include <vector>

#include "device/sifive_test.hpp"
#include "emulator.hpp"

namespace uemu::test {

constexpr size_t TEST_DRAM_SIZE = 32 * 1024 * 1024;

void test_file(const std::string& file, std::vector<std::string>& failed) {
    Emulator emulator(TEST_DRAM_SIZE);

    try {
        std::filesystem::path test_path = RISCV_TEST_DIR;
        test_path = test_path.parent_path() / file;
        emulator.loadelf(test_path.string());
    } catch (...) {
        std::cerr << "Exception happened while loading ELF. Skipping the test."
                  << std::endl;
        EXPECT_TRUE(false);
        return;
    }

    emulator.run();

    auto status = emulator.shutdown_status();

    if (status != device::SiFiveTest::Status::PASS)
        failed.emplace_back(file);

    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

void test_files(const std::vector<std::string>& files) {
    std::vector<std::string> failed;

    for (const auto& f : files)
        test_file(f, failed);

    if (!failed.empty()) {
        std::cerr << "Failed tests:\n";
        for (auto& f : failed)
            std::cerr << f << std::endl;
    }
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
        "riscv_test_riscv_tests_rv64mi_zicntr.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64SI) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64si_csr.elf",
        "riscv_test_riscv_tests_rv64si_dirty.elf",
        "riscv_test_riscv_tests_rv64si_icache-alias.elf",
        "riscv_test_riscv_tests_rv64si_ma_fetch.elf",
        "riscv_test_riscv_tests_rv64si_sbreak.elf",
        "riscv_test_riscv_tests_rv64si_scall.elf",
        "riscv_test_riscv_tests_rv64si_wfi.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UI) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64ui_add.elf",
        "riscv_test_riscv_tests_rv64ui_addi.elf",
        "riscv_test_riscv_tests_rv64ui_addiw.elf",
        "riscv_test_riscv_tests_rv64ui_addw.elf",
        "riscv_test_riscv_tests_rv64ui_and.elf",
        "riscv_test_riscv_tests_rv64ui_andi.elf",
        "riscv_test_riscv_tests_rv64ui_auipc.elf",
        "riscv_test_riscv_tests_rv64ui_beq.elf",
        "riscv_test_riscv_tests_rv64ui_bge.elf",
        "riscv_test_riscv_tests_rv64ui_bgeu.elf",
        "riscv_test_riscv_tests_rv64ui_blt.elf",
        "riscv_test_riscv_tests_rv64ui_bltu.elf",
        "riscv_test_riscv_tests_rv64ui_bne.elf",
        "riscv_test_riscv_tests_rv64ui_fence_i.elf",
        "riscv_test_riscv_tests_rv64ui_jal.elf",
        "riscv_test_riscv_tests_rv64ui_jalr.elf",
        "riscv_test_riscv_tests_rv64ui_lb.elf",
        "riscv_test_riscv_tests_rv64ui_lbu.elf",
        "riscv_test_riscv_tests_rv64ui_ld.elf",
        "riscv_test_riscv_tests_rv64ui_ld_st.elf",
        "riscv_test_riscv_tests_rv64ui_lh.elf",
        "riscv_test_riscv_tests_rv64ui_lhu.elf",
        "riscv_test_riscv_tests_rv64ui_lui.elf",
        "riscv_test_riscv_tests_rv64ui_lw.elf",
        "riscv_test_riscv_tests_rv64ui_lwu.elf",
        "riscv_test_riscv_tests_rv64ui_ma_data.elf",
        "riscv_test_riscv_tests_rv64ui_or.elf",
        "riscv_test_riscv_tests_rv64ui_ori.elf",
        "riscv_test_riscv_tests_rv64ui_sb.elf",
        "riscv_test_riscv_tests_rv64ui_sd.elf",
        "riscv_test_riscv_tests_rv64ui_sh.elf",
        "riscv_test_riscv_tests_rv64ui_simple.elf",
        "riscv_test_riscv_tests_rv64ui_sll.elf",
        "riscv_test_riscv_tests_rv64ui_slli.elf",
        "riscv_test_riscv_tests_rv64ui_slliw.elf",
        "riscv_test_riscv_tests_rv64ui_sllw.elf",
        "riscv_test_riscv_tests_rv64ui_slt.elf",
        "riscv_test_riscv_tests_rv64ui_slti.elf",
        "riscv_test_riscv_tests_rv64ui_sltiu.elf",
        "riscv_test_riscv_tests_rv64ui_sltu.elf",
        "riscv_test_riscv_tests_rv64ui_sra.elf",
        "riscv_test_riscv_tests_rv64ui_srai.elf",
        "riscv_test_riscv_tests_rv64ui_sraiw.elf",
        "riscv_test_riscv_tests_rv64ui_sraw.elf",
        "riscv_test_riscv_tests_rv64ui_srl.elf",
        "riscv_test_riscv_tests_rv64ui_srli.elf",
        "riscv_test_riscv_tests_rv64ui_srliw.elf",
        "riscv_test_riscv_tests_rv64ui_srlw.elf",
        "riscv_test_riscv_tests_rv64ui_st_ld.elf",
        "riscv_test_riscv_tests_rv64ui_sub.elf",
        "riscv_test_riscv_tests_rv64ui_subw.elf",
        "riscv_test_riscv_tests_rv64ui_sw.elf",
        "riscv_test_riscv_tests_rv64ui_xor.elf",
        "riscv_test_riscv_tests_rv64ui_xori.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UM) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64um_div.elf",
        "riscv_test_riscv_tests_rv64um_divu.elf",
        "riscv_test_riscv_tests_rv64um_divuw.elf",
        "riscv_test_riscv_tests_rv64um_divw.elf",
        "riscv_test_riscv_tests_rv64um_mul.elf",
        "riscv_test_riscv_tests_rv64um_mulh.elf",
        "riscv_test_riscv_tests_rv64um_mulhsu.elf",
        "riscv_test_riscv_tests_rv64um_mulhu.elf",
        "riscv_test_riscv_tests_rv64um_mulw.elf",
        "riscv_test_riscv_tests_rv64um_rem.elf",
        "riscv_test_riscv_tests_rv64um_remu.elf",
        "riscv_test_riscv_tests_rv64um_remuw.elf",
        "riscv_test_riscv_tests_rv64um_remw.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UA) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64ua_amoadd_d.elf",
        "riscv_test_riscv_tests_rv64ua_amoadd_w.elf",
        "riscv_test_riscv_tests_rv64ua_amoand_d.elf",
        "riscv_test_riscv_tests_rv64ua_amoand_w.elf",
        "riscv_test_riscv_tests_rv64ua_amomax_d.elf",
        "riscv_test_riscv_tests_rv64ua_amomaxu_d.elf",
        "riscv_test_riscv_tests_rv64ua_amomaxu_w.elf",
        "riscv_test_riscv_tests_rv64ua_amomax_w.elf",
        "riscv_test_riscv_tests_rv64ua_amomin_d.elf",
        "riscv_test_riscv_tests_rv64ua_amominu_d.elf",
        "riscv_test_riscv_tests_rv64ua_amominu_w.elf",
        "riscv_test_riscv_tests_rv64ua_amomin_w.elf",
        "riscv_test_riscv_tests_rv64ua_amoor_d.elf",
        "riscv_test_riscv_tests_rv64ua_amoor_w.elf",
        "riscv_test_riscv_tests_rv64ua_amoswap_d.elf",
        "riscv_test_riscv_tests_rv64ua_amoswap_w.elf",
        "riscv_test_riscv_tests_rv64ua_amoxor_d.elf",
        "riscv_test_riscv_tests_rv64ua_amoxor_w.elf",
        "riscv_test_riscv_tests_rv64ua_lrsc.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UF) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64uf_fadd.elf",
        "riscv_test_riscv_tests_rv64uf_fclass.elf",
        "riscv_test_riscv_tests_rv64uf_fcmp.elf",
        "riscv_test_riscv_tests_rv64uf_fcvt.elf",
        "riscv_test_riscv_tests_rv64uf_fcvt_w.elf",
        "riscv_test_riscv_tests_rv64uf_fdiv.elf",
        "riscv_test_riscv_tests_rv64uf_fmadd.elf",
        "riscv_test_riscv_tests_rv64uf_fmin.elf",
        "riscv_test_riscv_tests_rv64uf_ldst.elf",
        "riscv_test_riscv_tests_rv64uf_move.elf",
        "riscv_test_riscv_tests_rv64uf_recoding.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UD) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64ud_fadd.elf",
        "riscv_test_riscv_tests_rv64ud_fclass.elf",
        "riscv_test_riscv_tests_rv64ud_fcmp.elf",
        "riscv_test_riscv_tests_rv64ud_fcvt.elf",
        "riscv_test_riscv_tests_rv64ud_fcvt_w.elf",
        "riscv_test_riscv_tests_rv64ud_fdiv.elf",
        "riscv_test_riscv_tests_rv64ud_fmadd.elf",
        "riscv_test_riscv_tests_rv64ud_fmin.elf",
        "riscv_test_riscv_tests_rv64ud_ldst.elf",
        "riscv_test_riscv_tests_rv64ud_move.elf",
        "riscv_test_riscv_tests_rv64ud_recoding.elf",
        "riscv_test_riscv_tests_rv64ud_structural.elf",
    };

    test_files(files);
}

TEST(RISCVTest, RV64UC) {
    std::vector<std::string> files = {
        "riscv_test_riscv_tests_rv64uc_rvc.elf",
    };

    test_files(files);
}

}; // namespace uemu::test

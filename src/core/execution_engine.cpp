/*
 * Copyright 2026 Nuo Shen, Nanjing University
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

#include "core/execution_engine.hpp"
#include "common/bit.hpp"
#include "common/float.hpp"
#include "core/hart.hpp"

namespace uemu {

ExecutionEngine::ExecutionEngine(std::shared_ptr<core::Hart> hart,
                                 std::shared_ptr<core::Dram> dram,
                                 std::shared_ptr<core::Bus> bus,
                                 std::shared_ptr<core::MMU> mmu)
    : hart_(std::move(hart)), dram_(std::move(dram)), bus_(std::move(bus)),
      mmu_(std::move(mmu)) {
    cpu_thread_running_ = false;

    shutdown_from_guest_ = true;
    shutdown_code_ = shutdown_status_ = 0;

    shutdown_from_host_.store(false, std::memory_order::relaxed);

    // cache the pointers for faster emulation
    mcycle_ =
        dynamic_cast<core::MCYCLE*>(hart_->csrs[core::MCYCLE::ADDRESS].get());
    minstret_ = dynamic_cast<core::MINSTRET*>(
        hart_->csrs[core::MINSTRET::ADDRESS].get());
    assert(mcycle_ && minstret_);
}

ExecutionEngine::~ExecutionEngine() {
    if (cpu_thread_ && cpu_thread_->joinable()) {
        cpu_thread_->join();
        cpu_thread_.reset();
    }
}

void ExecutionEngine::execute_until_halt(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(cpu_mutex_);

    if (cpu_thread_running_)
        return;

    shutdown_from_guest_ = false;
    cpu_thread_ =
        std::make_unique<std::thread>(&ExecutionEngine::cpu_thread, this);
    cpu_cond_.wait(lock, [this]() -> bool {
        return cpu_thread_running_ || shutdown_from_guest_ ||
               shutdown_from_host_.load(std::memory_order::relaxed);
    });

    if (!cpu_thread_running_) {
        lock.unlock();
        return;
    }

    lock.unlock();

    auto start_time = std::chrono::steady_clock::now();
    bool timeout_enabled = timeout.count() > 0;

    while (true) {
        {
            std::lock_guard<std::mutex> lock(cpu_mutex_);
            if (!cpu_thread_running_)
                break;
        }

        // Check timeout
        if (timeout_enabled) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;

            if (elapsed >= timeout) {
                request_shutdown_from_host();
                std::println(
                    "Execution timeout reached ({} ms), shutting down...",
                    timeout.count());
                break;
            }
        }

        // Tick devices
        bus_->tick_devices();

        // Update UI
        if (ui_backend_) [[likely]]
            ui_backend_->update();

        std::this_thread::yield();
    }

    if (cpu_thread_exception_)
        std::rethrow_exception(cpu_thread_exception_);
}

void ExecutionEngine::request_shutdown_from_guest(uint16_t code,
                                                  uint16_t status) noexcept {
    shutdown_from_guest_ = true;
    shutdown_code_ = code;
    shutdown_status_ = status;
}

void ExecutionEngine::request_shutdown_from_host() noexcept {
    shutdown_from_host_.store(true, std::memory_order::relaxed);
}

void ExecutionEngine::cpu_thread() {
    using namespace core;

    {
        std::lock_guard<std::mutex> lock(cpu_mutex_);
        cpu_thread_running_ = true;
    }
    cpu_cond_.notify_all();

    // ====================================================================
    // Dispatch tables — initialized once (manual init, clang++/g++ compat)
    // ====================================================================

    // Primary dispatch by opcode[6:0]
    static void* dispatch[128];
    // Secondary dispatch by funct3[14:12]
    static void* op_imm_dispatch[8];
    static void* op_imm32_dispatch[8];
    static void* op_dispatch[8];
    static void* op32_dispatch[8];
    static void* m_op_dispatch[8];
    static void* m_op32_dispatch[8];
    static void* load_dispatch[8];
    static void* store_dispatch[8];
    static void* branch_dispatch[8];
    static void* system_dispatch[8];
    static void* load_fp_dispatch[8];
    static void* store_fp_dispatch[8];
    static void* op_fp_f7_dispatch[128];
    // Compressed dispatch
    static void* c_q0_dispatch[8];

    static bool tables_init = false;

    if (!tables_init) {
        // Primary (128 entries by opcode)
        for (int i = 0; i < 128; i++)
            dispatch[i] = &&L_illegal;
        dispatch[0b0000011] = &&L_load;
        dispatch[0b0000111] = &&L_load_fp;
        dispatch[0b0001111] = &&L_fence;
        dispatch[0b0010011] = &&L_op_imm;
        dispatch[0b0010111] = &&L_auipc;
        dispatch[0b0011011] = &&L_op_imm32;
        dispatch[0b0100011] = &&L_store;
        dispatch[0b0100111] = &&L_store_fp;
        dispatch[0b0101111] = &&L_amo;
        dispatch[0b0110011] = &&L_op;
        dispatch[0b0110111] = &&L_lui;
        dispatch[0b0111011] = &&L_op32;
        dispatch[0b1000011] = &&L_fmadd;
        dispatch[0b1000111] = &&L_fmsub;
        dispatch[0b1001011] = &&L_fnmsub;
        dispatch[0b1001111] = &&L_fnmadd;
        dispatch[0b1010011] = &&L_op_fp;
        dispatch[0b1100011] = &&L_branch;
        dispatch[0b1100111] = &&L_jalr;
        dispatch[0b1101111] = &&L_jal;
        dispatch[0b1110011] = &&L_system;

        // OP-IMM secondary
        op_imm_dispatch[0] = &&L_addi;
        op_imm_dispatch[1] = &&L_slli;
        op_imm_dispatch[2] = &&L_slti;
        op_imm_dispatch[3] = &&L_sltiu;
        op_imm_dispatch[4] = &&L_xori;
        op_imm_dispatch[5] = &&L_srxi;
        op_imm_dispatch[6] = &&L_ori;
        op_imm_dispatch[7] = &&L_andi;

        // OP-IMM-32 secondary
        op_imm32_dispatch[0] = &&L_addiw;
        op_imm32_dispatch[1] = &&L_slliw;
        op_imm32_dispatch[2] = &&L_illegal;
        op_imm32_dispatch[3] = &&L_illegal;
        op_imm32_dispatch[4] = &&L_illegal;
        op_imm32_dispatch[5] = &&L_srxi32;
        op_imm32_dispatch[6] = &&L_illegal;
        op_imm32_dispatch[7] = &&L_illegal;

        // OP secondary
        op_dispatch[0] = &&L_opsub;
        op_dispatch[1] = &&L_sll;
        op_dispatch[2] = &&L_slt;
        op_dispatch[3] = &&L_sltu;
        op_dispatch[4] = &&L_xor;
        op_dispatch[5] = &&L_srxs;
        op_dispatch[6] = &&L_or;
        op_dispatch[7] = &&L_and;

        // OP-32 secondary
        op32_dispatch[0] = &&L_opsub32;
        op32_dispatch[1] = &&L_sllw;
        op32_dispatch[2] = &&L_illegal;
        op32_dispatch[3] = &&L_illegal;
        op32_dispatch[4] = &&L_illegal;
        op32_dispatch[5] = &&L_srxs32;
        op32_dispatch[6] = &&L_illegal;
        op32_dispatch[7] = &&L_illegal;

        // M (OP) secondary (bit 25 = 1)
        m_op_dispatch[0] = &&L_mul;
        m_op_dispatch[1] = &&L_mulh;
        m_op_dispatch[2] = &&L_mulhsu;
        m_op_dispatch[3] = &&L_mulhu;
        m_op_dispatch[4] = &&L_div;
        m_op_dispatch[5] = &&L_divu;
        m_op_dispatch[6] = &&L_rem;
        m_op_dispatch[7] = &&L_remu;

        // M-32 (OP-32) secondary (bit 25 = 1)
        m_op32_dispatch[0] = &&L_mulw;
        m_op32_dispatch[1] = &&L_illegal;
        m_op32_dispatch[2] = &&L_illegal;
        m_op32_dispatch[3] = &&L_illegal;
        m_op32_dispatch[4] = &&L_divw;
        m_op32_dispatch[5] = &&L_divuw;
        m_op32_dispatch[6] = &&L_remw;
        m_op32_dispatch[7] = &&L_remuw;

        // LOAD secondary
        load_dispatch[0] = &&L_lb;
        load_dispatch[1] = &&L_lh;
        load_dispatch[2] = &&L_lw;
        load_dispatch[3] = &&L_ld;
        load_dispatch[4] = &&L_lbu;
        load_dispatch[5] = &&L_lhu;
        load_dispatch[6] = &&L_lwu;
        load_dispatch[7] = &&L_illegal;

        // STORE secondary
        store_dispatch[0] = &&L_sb;
        store_dispatch[1] = &&L_sh;
        store_dispatch[2] = &&L_sw;
        store_dispatch[3] = &&L_sd;
        store_dispatch[4] = &&L_illegal;
        store_dispatch[5] = &&L_illegal;
        store_dispatch[6] = &&L_illegal;
        store_dispatch[7] = &&L_illegal;

        // BRANCH secondary
        branch_dispatch[0] = &&L_beq;
        branch_dispatch[1] = &&L_bne;
        branch_dispatch[2] = &&L_illegal;
        branch_dispatch[3] = &&L_illegal;
        branch_dispatch[4] = &&L_blt;
        branch_dispatch[5] = &&L_bge;
        branch_dispatch[6] = &&L_bltu;
        branch_dispatch[7] = &&L_bgeu;

        // SYSTEM secondary (CSR by funct3)
        system_dispatch[1] = &&L_csrrw;
        system_dispatch[2] = &&L_csrrs;
        system_dispatch[3] = &&L_csrrc;
        system_dispatch[5] = &&L_csrrwi;
        system_dispatch[6] = &&L_csrrsi;
        system_dispatch[7] = &&L_csrrci;

        // LOAD-FP secondary
        load_fp_dispatch[2] = &&L_flw;
        load_fp_dispatch[3] = &&L_fld;

        // STORE-FP secondary
        store_fp_dispatch[2] = &&L_fsw;
        store_fp_dispatch[3] = &&L_fsd;

        // OP-FP secondary: 128-entry by funct7[6:0]
        for (int i = 0; i < 128; i++)
            op_fp_f7_dispatch[i] = &&L_illegal;
        // fadd: funct7=0000000(S) 0000001(D)
        op_fp_f7_dispatch[0b0000000] = &&L_fadd_s;
        op_fp_f7_dispatch[0b0000001] = &&L_fadd_d;
        // fsub: funct7=0000100(S) 0000101(D)
        op_fp_f7_dispatch[0b0000100] = &&L_fsub_s;
        op_fp_f7_dispatch[0b0000101] = &&L_fsub_d;
        // fmul: funct7=0001000(S) 0001001(D)
        op_fp_f7_dispatch[0b0001000] = &&L_fmul_s;
        op_fp_f7_dispatch[0b0001001] = &&L_fmul_d;
        // fdiv: funct7=0001100(S) 0001101(D)
        op_fp_f7_dispatch[0b0001100] = &&L_fdiv_s;
        op_fp_f7_dispatch[0b0001101] = &&L_fdiv_d;
        // fsqrt: funct7=0101100(S) 0101101(D)
        op_fp_f7_dispatch[0b0101100] = &&L_fsqrt_s;
        op_fp_f7_dispatch[0b0101101] = &&L_fsqrt_d;
        // fsgnj: funct7=0010000(S) 0010001(D), funct3=000/001/010 for J/JN/JX
        op_fp_f7_dispatch[0b0010000] = &&L_fsgnj_s;
        op_fp_f7_dispatch[0b0010001] = &&L_fsgnj_d;
        // fsgnjn: (handled inside fsgnj labels via funct3 check)
        // fsgnjx: (handled inside fsgnj labels via funct3 check)
        // fmin/fmax: funct7=0010100(S) 0010101(D), funct3=000(min)/001(max)
        op_fp_f7_dispatch[0b0010100] = &&L_fminmax_s;
        op_fp_f7_dispatch[0b0010101] = &&L_fminmax_d;
        // fcvt_w/wu/l/lu from S: funct7=1100000, rs2=00000/00001/00010/00011
        op_fp_f7_dispatch[0b1100000] = &&L_fcvt_x_s;
        // fcvt_w/wu/l/lu from D: funct7=1100001
        op_fp_f7_dispatch[0b1100001] = &&L_fcvt_x_d;
        // fcvt to S w/wu/l/lu: funct7=1101000
        op_fp_f7_dispatch[0b1101000] = &&L_fcvt_s_x;
        // fcvt to D w/wu/l/lu: funct7=1101001
        op_fp_f7_dispatch[0b1101001] = &&L_fcvt_d_x;
        // fcvt s→d: funct7=0100000, rs2=00001; d→s: funct7=0100001, rs2=00000
        op_fp_f7_dispatch[0b0100000] = &&L_fcvt_s_d;
        op_fp_f7_dispatch[0b0100001] = &&L_fcvt_d_s;
        // fmv_x_w/d or fclass: funct7=1110000(S) 1110001(D)
        op_fp_f7_dispatch[0b1110000] = &&L_fmv_x_fclass_s;
        op_fp_f7_dispatch[0b1110001] = &&L_fmv_x_fclass_d;
        // fmv_w/d_x: funct7=1111000(S) 1111001(D)
        op_fp_f7_dispatch[0b1111000] = &&L_fmv_w_x;
        op_fp_f7_dispatch[0b1111001] = &&L_fmv_d_x;
        // feq/flt/fle: funct7=1010000(S) 1010001(D), funct3=010/001/000
        op_fp_f7_dispatch[0b1010000] = &&L_fcmp_s;
        op_fp_f7_dispatch[0b1010001] = &&L_fcmp_d;

        // Compressed quadrant 0 dispatch
        c_q0_dispatch[0] = &&L_c_q0_ciw;
        c_q0_dispatch[1] = &&L_c_q0_fld;
        c_q0_dispatch[2] = &&L_c_q0_lw;
        c_q0_dispatch[3] = &&L_c_q0_ld;
        c_q0_dispatch[4] = &&L_c_inv; // reserved
        c_q0_dispatch[5] = &&L_c_q0_fsd;
        c_q0_dispatch[6] = &&L_c_q0_sw;
        c_q0_dispatch[7] = &&L_c_q0_sd;

        tables_init = true;
    }

// ====================================================================
// Exec macros (textual inclusion — must precede label files)
// ====================================================================
#include "local-include/insn_exec_macros.hpp"

    // ====================================================================
    // Helpers
    // ====================================================================
#define REG_C(x) ((x) + 8)

    // ====================================================================
    // Local variables (referenced by macros and labels)
    // ====================================================================
    auto* hart = hart_.get();
    auto* mmu = mmu_.get();
    auto& R = hart_->gprs;
    auto& F = hart_->fprs;
    auto& csrs = hart_->csrs;

    int8_t rd = 0, rs1 = 0, rs2 = 0, rs3 = 0;
    int64_t imm = 0;
    uint8_t funct3 = 0, funct7 = 0, shamt = 0;
    size_t csr = 0;
    uint32_t insn = 0;
    addr_t pc = 0;

    // ====================================================================
    // Main execution loop
    // ====================================================================
L_reentry:
    if (shutdown_from_guest_) [[unlikely]]
        goto L_done;
    if (shutdown_from_host_.load(std::memory_order_relaxed)) [[unlikely]]
        goto L_done;
    mcycle_->advance();

    try {
        hart_->check_interrupts();

        pc = hart->pc;
        {
            auto [if_insn, ilen] = mmu_->ifetch();
            insn = if_insn;
            hart->pc = pc + static_cast<addr_t>(ilen);
        }

        // Dispatch: compressed (16-bit) vs standard (32-bit)
        if ((insn & 0x3) != 0x3)
            goto L_compressed;
        goto* dispatch[insn & 0x7F];

        // ============================================================
        // Group-level decode labels
        // ============================================================

        // === Group Labels (32-bit instruction dispatch) ===

    L_op_imm:
        funct3 = bits(insn, 14, 12);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        shamt = bits(insn, 25, 20);
        goto* op_imm_dispatch[funct3];

    L_srxi:
        if (bits(insn, 30, 30))
            goto L_srai;
        goto L_srli;

    L_op:
        funct3 = bits(insn, 14, 12);
        funct7 = bits(insn, 31, 25);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        if (funct7 & 0x01)
            goto* m_op_dispatch[funct3]; // M extension (bit 25)
        goto* op_dispatch[funct3];

    L_opsub:
        if (bits(insn, 30, 30))
            goto L_sub;
        goto L_add;

    L_srxs:
        if (bits(insn, 30, 30))
            goto L_sra;
        goto L_srl;

    L_op_imm32:
        funct3 = bits(insn, 14, 12);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        goto* op_imm32_dispatch[funct3];

    L_srxi32:
        if (bits(insn, 30, 30))
            goto L_sraiw;
        goto L_srliw;

    L_op32:
        funct3 = bits(insn, 14, 12);
        funct7 = bits(insn, 31, 25);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        if (funct7 & 0x01)
            goto* m_op32_dispatch[funct3]; // M-32 extension (bit 25)
        goto* op32_dispatch[funct3];

    L_opsub32:
        if (bits(insn, 30, 30))
            goto L_subw;
        goto L_addw;

    L_srxs32:
        if (bits(insn, 30, 30))
            goto L_sraw;
        goto L_srlw;

    L_load:
        funct3 = bits(insn, 14, 12);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        goto* load_dispatch[funct3];

    L_store:
        funct3 = bits(insn, 14, 12);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        imm = (sext(bits(insn, 31, 25), 7) << 5) | bits(insn, 11, 7);
        goto* store_dispatch[funct3];

    L_branch:
        funct3 = bits(insn, 14, 12);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        imm = sext(bits(insn, 31, 31) << 12 | bits(insn, 7, 7) << 11 |
                       bits(insn, 30, 25) << 5 | bits(insn, 11, 8) << 1,
                   13);
        goto* branch_dispatch[funct3];

    L_jal:
        rd = bits(insn, 11, 7);
        imm = sext(bits(insn, 31, 31) << 20 | bits(insn, 19, 12) << 12 |
                       bits(insn, 20, 20) << 11 | bits(insn, 30, 21) << 1,
                   21);
        EXEC_JAL(); // contains goto L_insn_done
        // unreachable

    L_jalr:
        funct3 = bits(insn, 14, 12);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        EXEC_JALR(); // contains goto L_insn_done
        // unreachable

    L_lui:
        rd = bits(insn, 11, 7);
        imm = sext(bits(insn, 31, 12), 20) << 12;
        EXEC_LUI();
        goto L_insn_done;

    L_auipc:
        rd = bits(insn, 11, 7);
        imm = sext(bits(insn, 31, 12), 20) << 12;
        EXEC_AUIPC();
        goto L_insn_done;

    L_fence:
        funct3 = bits(insn, 14, 12);
        if (funct3 == 0b000) {
            EXEC_FENCE();
            goto L_insn_done;
        }
        if (funct3 == 0b001) {
            EXEC_FENCE_I();
            goto L_insn_done;
        }
        goto L_illegal;

        // === SYSTEM (opcode 1110011) ===

    L_system:
        funct3 = bits(insn, 14, 12);
        if (funct3 == 0b000) {
            // Privileged instructions — match by full insn
            if (insn == 0x00000073) {
                EXEC_ECALL();
                goto L_insn_done;
            } // ecall
            if (insn == 0x00100073) {
                EXEC_EBREAK();
                goto L_insn_done;
            } // ebreak
            if (insn == 0x30200073) {
                EXEC_MRET();
            } // mret (has goto L_insn_done)
            if (insn == 0x10200073) {
                EXEC_SRET();
            } // sret (has goto L_insn_done)
            if (insn == 0x10500073) {
                EXEC_WFI(); /* may throw */
                goto L_insn_done;
            }
            // sfence_vma: funct7=0001001, rs2=rs1 (but rs1 is variable)
            if ((insn & 0xFE007FFF) == 0x12000073) {
                rs1 = bits(insn, 19, 15);
                EXEC_SFENCE_VMA();
                goto L_insn_done;
            }
            goto L_illegal;
        }
        // CSR instructions
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        csr = imm & 0xFFF;
        goto* system_dispatch[funct3];

        // === AMO (opcode 0101111) ===

    L_amo:
        funct3 = bits(insn, 14, 12);
        funct7 = bits(insn, 31, 27);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        if (funct3 == 0b010)
            goto L_amo_w; // AMO word
        if (funct3 == 0b011)
            goto L_amo_d; // AMO double
        goto L_illegal;

    L_amo_w:
        if (funct7 == 0b00010) {
            EXEC_LR_W();
            goto L_insn_done;
        }
        if (funct7 == 0b00011) {
            EXEC_SC_W();
            goto L_insn_done;
        }
        if (funct7 == 0b00000) {
            EXEC_AMOADD_W();
            goto L_insn_done;
        }
        if (funct7 == 0b00001) {
            EXEC_AMOSWAP_W();
            goto L_insn_done;
        }
        if (funct7 == 0b00100) {
            EXEC_AMOXOR_W();
            goto L_insn_done;
        }
        if (funct7 == 0b01100) {
            EXEC_AMOAND_W();
            goto L_insn_done;
        }
        if (funct7 == 0b01000) {
            EXEC_AMOOR_W();
            goto L_insn_done;
        }
        if (funct7 == 0b10000) {
            EXEC_AMOMIN_W();
            goto L_insn_done;
        }
        if (funct7 == 0b10100) {
            EXEC_AMOMAX_W();
            goto L_insn_done;
        }
        if (funct7 == 0b11000) {
            EXEC_AMOMINU_W();
            goto L_insn_done;
        }
        if (funct7 == 0b11100) {
            EXEC_AMOMAXU_W();
            goto L_insn_done;
        }
        goto L_illegal;

    L_amo_d:
        if (funct7 == 0b00010) {
            EXEC_LR_D();
            goto L_insn_done;
        }
        if (funct7 == 0b00011) {
            EXEC_SC_D();
            goto L_insn_done;
        }
        if (funct7 == 0b00000) {
            EXEC_AMOADD_D();
            goto L_insn_done;
        }
        if (funct7 == 0b00001) {
            EXEC_AMOSWAP_D();
            goto L_insn_done;
        }
        if (funct7 == 0b00100) {
            EXEC_AMOXOR_D();
            goto L_insn_done;
        }
        if (funct7 == 0b01100) {
            EXEC_AMOAND_D();
            goto L_insn_done;
        }
        if (funct7 == 0b01000) {
            EXEC_AMOOR_D();
            goto L_insn_done;
        }
        if (funct7 == 0b10000) {
            EXEC_AMOMIN_D();
            goto L_insn_done;
        }
        if (funct7 == 0b10100) {
            EXEC_AMOMAX_D();
            goto L_insn_done;
        }
        if (funct7 == 0b11000) {
            EXEC_AMOMINU_D();
            goto L_insn_done;
        }
        if (funct7 == 0b11100) {
            EXEC_AMOMAXU_D();
            goto L_insn_done;
        }
        goto L_illegal;

        // === LOAD-FP (opcode 0000111) ===

    L_load_fp:
        funct3 = bits(insn, 14, 12);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        imm = sext(bits(insn, 31, 20), 12);
        goto* load_fp_dispatch[funct3];

        // === STORE-FP (opcode 0100111) ===

    L_store_fp:
        funct3 = bits(insn, 14, 12);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        imm = (sext(bits(insn, 31, 25), 7) << 5) | bits(insn, 11, 7);
        goto* store_fp_dispatch[funct3];

        // === OP-FP (opcode 1010011) ===
        // FP instructions use funct7 for operation selection; funct3 = rm
        // (rounding mode) which is handled inside the EXEC macros via
        // FP_SETUP_RM().

    L_op_fp:
        funct7 = bits(insn, 31, 25);
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        rs3 = bits(insn, 31, 27);
        goto* op_fp_f7_dispatch[funct7]; // 128-entry by full funct7

        // === FMADD/FMSUB/FNMSUB/FNMADD (opcodes 1000011, 1000111, 1001011,
        // 1001111) ===

    L_fmadd:
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        rs3 = bits(insn, 31, 27);
        if (bits(insn, 25, 25))
            goto L_fmadd_d;
        goto L_fmadd_s;

    L_fmsub:
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        rs3 = bits(insn, 31, 27);
        if (bits(insn, 25, 25))
            goto L_fmsub_d;
        goto L_fmsub_s;

    L_fnmsub:
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        rs3 = bits(insn, 31, 27);
        if (bits(insn, 25, 25))
            goto L_fnmsub_d;
        goto L_fnmsub_s;

    L_fnmadd:
        rd = bits(insn, 11, 7);
        rs1 = bits(insn, 19, 15);
        rs2 = bits(insn, 24, 20);
        rs3 = bits(insn, 31, 27);
        if (bits(insn, 25, 25))
            goto L_fnmadd_d;
        goto L_fnmadd_s;

        // === Individual Instruction Labels — RV64I Base Integer ===

    L_add:
        EXEC_ADD();
        goto L_insn_done;
    L_addi:
        EXEC_ADDI();
        goto L_insn_done;
    L_addiw:
        EXEC_ADDIW();
        goto L_insn_done;
    L_addw:
        EXEC_ADDW();
        goto L_insn_done;
    L_and:
        EXEC_AND();
        goto L_insn_done;
    L_andi:
        EXEC_ANDI();
        goto L_insn_done;
    L_or:
        EXEC_OR();
        goto L_insn_done;
    L_ori:
        EXEC_ORI();
        goto L_insn_done;
    L_xor:
        EXEC_XOR();
        goto L_insn_done;
    L_xori:
        EXEC_XORI();
        goto L_insn_done;
    L_sll:
        EXEC_SLL();
        goto L_insn_done;
    L_slli:
        EXEC_SLLI();
        goto L_insn_done;
    L_slliw:
        EXEC_SLLIW();
        goto L_insn_done;
    L_sllw:
        EXEC_SLLW();
        goto L_insn_done;
    L_srl:
        EXEC_SRL();
        goto L_insn_done;
    L_srli:
        EXEC_SRLI();
        goto L_insn_done;
    L_srliw:
        EXEC_SRLIW();
        goto L_insn_done;
    L_srlw:
        EXEC_SRLW();
        goto L_insn_done;
    L_sra:
        EXEC_SRA();
        goto L_insn_done;
    L_srai:
        EXEC_SRAI();
        goto L_insn_done;
    L_sraiw:
        EXEC_SRAIW();
        goto L_insn_done;
    L_sraw:
        EXEC_SRAW();
        goto L_insn_done;
    L_slt:
        EXEC_SLT();
        goto L_insn_done;
    L_slti:
        EXEC_SLTI();
        goto L_insn_done;
    L_sltiu:
        EXEC_SLTIU();
        goto L_insn_done;
    L_sltu:
        EXEC_SLTU();
        goto L_insn_done;
    L_sub:
        EXEC_SUB();
        goto L_insn_done;
    L_subw:
        EXEC_SUBW();
        goto L_insn_done;

    L_lb:
        EXEC_LB();
        goto L_insn_done;
    L_lh:
        EXEC_LH();
        goto L_insn_done;
    L_lw:
        EXEC_LW();
        goto L_insn_done;
    L_ld:
        EXEC_LD();
        goto L_insn_done;
    L_lbu:
        EXEC_LBU();
        goto L_insn_done;
    L_lhu:
        EXEC_LHU();
        goto L_insn_done;
    L_lwu:
        EXEC_LWU();
        goto L_insn_done;

    L_sb:
        EXEC_SB();
        goto L_insn_done;
    L_sh:
        EXEC_SH();
        goto L_insn_done;
    L_sw:
        EXEC_SW();
        goto L_insn_done;
    L_sd:
        EXEC_SD();
        goto L_insn_done;

    L_beq:
        EXEC_BEQ();
        goto L_insn_done;
    L_bne:
        EXEC_BNE();
        goto L_insn_done;
    L_blt:
        EXEC_BLT();
        goto L_insn_done;
    L_bge:
        EXEC_BGE();
        goto L_insn_done;
    L_bltu:
        EXEC_BLTU();
        goto L_insn_done;
    L_bgeu:
        EXEC_BGEU();
        goto L_insn_done;

        // === Compressed Instruction Dispatch ===
        // Compressed instructions are 16-bit; dispatched via quadrant + funct3
        // hierarchy. Field extraction follows decode_operand_16() logic from
        // decoder.cpp.

        // ============================================================
        // Top-level compressed dispatch
        // ============================================================

    L_compressed:
        switch (insn & 0x3) {
            case 0b00: goto L_c_q0; // CIW, CL, CS, CA, CB
            case 0b01: goto L_c_q1; // CI, CJ, CB, CA
            case 0b10: goto L_c_q2; // CI, CR, CSS
            default: goto L_c_inv;
        }

        // ============================================================
        // Quadrant 0 (insn[1:0] = 00): CIW, CL, CS, CA, CB
        // ============================================================

    L_c_q0:
        funct3 = bits(insn, 15, 13);
        goto* c_q0_dispatch[funct3];

    // funct3=000: CIW (c_addi4spn) or illegal (c_inv)
    L_c_q0_ciw:
        if (bits(insn, 12, 5) == 0)
            goto L_c_inv; // c_inv: all zeros
        rd = REG_C(bits(insn, 4, 2));
        imm = bits(insn, 10, 7) << 6 | bits(insn, 12, 11) << 4 |
              bits(insn, 5, 5) << 3 | bits(insn, 6, 6) << 2;
        EXEC_C_ADDI4SPN();
        goto L_insn_done;

    // funct3=001: CL (c_fld)
    L_c_q0_fld:
        rd = REG_C(bits(insn, 4, 2));
        rs1 = REG_C(bits(insn, 9, 7));
        imm = (bits(insn, 6, 5) << 6) | (bits(insn, 12, 10) << 3);
        EXEC_C_FLD();
        goto L_insn_done;

    // funct3=010: CL (c_lw)
    L_c_q0_lw:
        rd = REG_C(bits(insn, 4, 2));
        rs1 = REG_C(bits(insn, 9, 7));
        imm = (bits(insn, 5, 5) << 6) | (bits(insn, 12, 10) << 3) |
              (bits(insn, 6, 6) << 2);
        EXEC_C_LW();
        goto L_insn_done;

    // funct3=011: CL (c_ld)
    L_c_q0_ld:
        rd = REG_C(bits(insn, 4, 2));
        rs1 = REG_C(bits(insn, 9, 7));
        imm = (bits(insn, 6, 5) << 6) | (bits(insn, 12, 10) << 3);
        EXEC_C_LD();
        goto L_insn_done;

    // funct3=100: reserved → illegal
    // funct3=101: CS (c_fsd)
    L_c_q0_fsd:
        rs1 = REG_C(bits(insn, 9, 7));
        rs2 = REG_C(bits(insn, 4, 2));
        imm = (bits(insn, 6, 5) << 6) | (bits(insn, 12, 10) << 3);
        EXEC_C_FSD();
        goto L_insn_done;

    // funct3=110: CS (c_sw)
    L_c_q0_sw:
        rs1 = REG_C(bits(insn, 9, 7));
        rs2 = REG_C(bits(insn, 4, 2));
        imm = (bits(insn, 5, 5) << 6) | (bits(insn, 12, 10) << 3) |
              (bits(insn, 6, 6) << 2);
        EXEC_C_SW();
        goto L_insn_done;

    // funct3=111: CS (c_sd)
    L_c_q0_sd:
        rs1 = REG_C(bits(insn, 9, 7));
        rs2 = REG_C(bits(insn, 4, 2));
        imm = (bits(insn, 6, 5) << 6) | (bits(insn, 12, 10) << 3);
        EXEC_C_SD();
        goto L_insn_done;

        // ============================================================
        // Quadrant 1 (insn[1:0] = 01): CI, CJ, CB, CA
        // ============================================================

    L_c_q1:
        funct3 = bits(insn, 15, 13);
        if (funct3 == 0b000) { // CI: c_addi or c_nop
            rd = rs1 = bits(insn, 11, 7);
            imm = sext(bits(insn, 12, 12) << 5 | bits(insn, 6, 2), 6);
            if (rd == 0) {
                EXEC_C_NOP();
                goto L_insn_done;
            }
            EXEC_C_ADDI();
            goto L_insn_done;
        }
        if (funct3 == 0b001 || funct3 == 0b010) { // CI: c_addiw or c_li
            rd = rs1 = bits(insn, 11, 7);
            imm = sext(bits(insn, 12, 12) << 5 | bits(insn, 6, 2), 6);
            if (funct3 == 0b001) {
                EXEC_C_ADDIW();
                goto L_insn_done;
            }
            EXEC_C_LI();
            goto L_insn_done;
        }
        if (funct3 == 0b011) { // CI: c_addi16sp or c_lui
            rd = bits(insn, 11, 7);
            if (rd == 2) { // c_addi16sp
                imm = sext(bits(insn, 12, 12) << 9 | bits(insn, 4, 3) << 7 |
                               bits(insn, 5, 5) << 6 | bits(insn, 2, 2) << 5 |
                               bits(insn, 6, 6) << 4,
                           10);
                EXEC_C_ADDI16SP();
                goto L_insn_done;
            } else { // c_lui (rd=0 is HINT/NOP per RISC-V spec)
                imm =
                    sext(bits(insn, 12, 12) << 17 | bits(insn, 6, 2) << 12, 18);
                EXEC_C_LUI();
                goto L_insn_done;
            }
        }
        if (funct3 == 0b100)
            goto L_c_q1_f4;    // CB or CA (by bit 11:10)
        if (funct3 == 0b101) { // CJ: c_j
            imm = sext(bits(insn, 12, 12) << 11 | bits(insn, 11, 11) << 4 |
                           bits(insn, 10, 9) << 8 | bits(insn, 8, 8) << 10 |
                           bits(insn, 7, 7) << 6 | bits(insn, 6, 6) << 7 |
                           bits(insn, 5, 3) << 1 | bits(insn, 2, 2) << 5,
                       12);
            EXEC_C_J(); // contains goto L_insn_done
        }
        if (funct3 == 0b110 || funct3 == 0b111) { // CB: c_beqz / c_bnez
            rd = rs1 = REG_C(bits(insn, 9, 7));
            imm = sext(bits(insn, 12, 12) << 8 | bits(insn, 6, 5) << 6 |
                           bits(insn, 2, 2) << 5 | bits(insn, 11, 10) << 3 |
                           bits(insn, 4, 3) << 1,
                       9);
            if (funct3 == 0b110) {
                EXEC_C_BEQZ();
                goto L_insn_done;
            }
            EXEC_C_BNEZ();
            goto L_insn_done;
        }
        goto L_c_inv;

    // funct3=100 sub-dispatch: CB (srli/srai/andi) or CA
    // (sub/xor/or/and/subw/addw)
    L_c_q1_f4:
        // Distinguish: bits[11:10]=00/01/10 → CB, bits[11:10]=11 → CA
        if (bits(insn, 11, 10) != 0b11) {
            // CB: c_srli, c_srai, c_andi
            rd = rs1 = REG_C(bits(insn, 9, 7));
            uint32_t raw = (bits(insn, 12, 12) << 5) | bits(insn, 6, 2);
            if (bits(insn, 11, 10) == 0b00) {
                // c_srli
                imm = raw;
                EXEC_C_SRLI();
                goto L_insn_done;
            } else if (bits(insn, 11, 10) == 0b01) {
                // c_srai
                imm = raw;
                EXEC_C_SRAI();
                goto L_insn_done;
            } else {
                // c_andi
                imm = sext(raw, 6);
                EXEC_C_ANDI();
                goto L_insn_done;
            }
        } else {
            // CA: c_sub, c_xor, c_or, c_and, c_subw, c_addw
            rd = rs1 = REG_C(bits(insn, 9, 7));
            rs2 = REG_C(bits(insn, 4, 2));
            uint8_t f2 = bits(insn, 6, 5);
            if (bits(insn, 12, 12) == 0) { // arithmetic
                if (f2 == 0b00) {
                    EXEC_C_SUB();
                    goto L_insn_done;
                }
                if (f2 == 0b01) {
                    EXEC_C_XOR();
                    goto L_insn_done;
                }
                if (f2 == 0b10) {
                    EXEC_C_OR();
                    goto L_insn_done;
                }
                if (f2 == 0b11) {
                    EXEC_C_AND();
                    goto L_insn_done;
                }
            } else { // word arithmetic
                if (f2 == 0b00) {
                    EXEC_C_SUBW();
                    goto L_insn_done;
                }
                if (f2 == 0b01) {
                    EXEC_C_ADDW();
                    goto L_insn_done;
                }
            }
            goto L_c_inv;
        }

        // ============================================================
        // Quadrant 2 (insn[1:0] = 10): CI, CR, CSS
        // ============================================================

    L_c_q2:
        funct3 = bits(insn, 15, 13);
        if (funct3 == 0b000) { // CI: c_slli
            rd = rs1 = bits(insn, 11, 7);
            imm = (bits(insn, 12, 12) << 5) | bits(insn, 6, 2);
            EXEC_C_SLLI();
            goto L_insn_done;
        }
        if (funct3 == 0b001 || funct3 == 0b011) { // CI: c_fldsp / c_ldsp
            rd = bits(insn, 11, 7);
            if (funct3 == 0b001) {
                imm = bits(insn, 4, 2) << 6 | bits(insn, 12, 12) << 5 |
                      bits(insn, 6, 5) << 3;
                EXEC_C_FLDSP();
                goto L_insn_done;
            } else {
                imm = bits(insn, 4, 2) << 6 | bits(insn, 12, 12) << 5 |
                      bits(insn, 6, 5) << 3;
                EXEC_C_LDSP();
                goto L_insn_done;
            }
        }
        if (funct3 == 0b010) { // CI: c_lwsp
            rd = bits(insn, 11, 7);
            imm = bits(insn, 3, 2) << 6 | bits(insn, 12, 12) << 5 |
                  bits(insn, 6, 4) << 2;
            EXEC_C_LWSP();
            goto L_insn_done;
        }
        if (funct3 == 0b100) {             // CR
            if (bits(insn, 12, 12) == 0) { // c_jr or c_mv
                rd = bits(insn, 11, 7);
                rs1 = bits(insn, 11, 7);
                rs2 = bits(insn, 6, 2);
                if (rs2 == 0) {
                    EXEC_C_JR(); // contains goto L_insn_done
                } else {
                    EXEC_C_MV();
                    goto L_insn_done;
                }
            } else { // c_ebreak or c_jalr or c_add
                rd = bits(insn, 11, 7);
                rs1 = bits(insn, 11, 7);
                rs2 = bits(insn, 6, 2);
                if (rs2 == 0) {
                    if (rs1 == 0) { // c_ebreak
                        EXEC_C_EBREAK();
                        goto L_insn_done;
                    } else {           // c_jalr
                        EXEC_C_JALR(); // contains goto L_insn_done
                    }
                } else { // c_add
                    EXEC_C_ADD();
                    goto L_insn_done;
                }
            }
        }
        if (funct3 == 0b101) { // CSS: c_fsdsp
            rs1 = 2;           // sp
            rs2 = bits(insn, 6, 2);
            imm = bits(insn, 9, 7) << 6 | bits(insn, 12, 10) << 3;
            EXEC_C_FSDSP();
            goto L_insn_done;
        }
        if (funct3 == 0b110) { // CSS: c_swsp
            rs1 = 2;           // sp
            rs2 = bits(insn, 6, 2);
            imm = bits(insn, 8, 7) << 6 | bits(insn, 12, 9) << 2;
            EXEC_C_SWSP();
            goto L_insn_done;
        }
        if (funct3 == 0b111) { // CSS: c_sdsp
            rs1 = 2;           // sp
            rs2 = bits(insn, 6, 2);
            imm = bits(insn, 9, 7) << 6 | bits(insn, 12, 10) << 3;
            EXEC_C_SDSP();
            goto L_insn_done;
        }
        goto L_c_inv;

        // ============================================================
        // Invalid compressed instruction
        // ============================================================

    L_c_inv:
        goto L_illegal;

        // === CSR Instructions ===

    L_csrrw:
        EXEC_CSRRW();
        goto L_insn_done;
    L_csrrs:
        EXEC_CSRRS();
        goto L_insn_done;
    L_csrrc:
        EXEC_CSRRC();
        goto L_insn_done;
    L_csrrwi:
        EXEC_CSRRWI();
        goto L_insn_done;
    L_csrrsi:
        EXEC_CSRRSI();
        goto L_insn_done;
    L_csrrci:
        EXEC_CSRRCI();
        goto L_insn_done;

        // === RV64M Extension ===

    L_mul:
        EXEC_MUL();
        goto L_insn_done;
    L_mulh:
        EXEC_MULH();
        goto L_insn_done;
    L_mulhsu:
        EXEC_MULHSU();
        goto L_insn_done;
    L_mulhu:
        EXEC_MULHU();
        goto L_insn_done;
    L_mulw:
        EXEC_MULW();
        goto L_insn_done;
    L_div:
        EXEC_DIV();
        goto L_insn_done;
    L_divu:
        EXEC_DIVU();
        goto L_insn_done;
    L_divw:
        EXEC_DIVW();
        goto L_insn_done;
    L_divuw:
        EXEC_DIVUW();
        goto L_insn_done;
    L_rem:
        EXEC_REM();
        goto L_insn_done;
    L_remu:
        EXEC_REMU();
        goto L_insn_done;
    L_remw:
        EXEC_REMW();
        goto L_insn_done;
    L_remuw:
        EXEC_REMUW();
        goto L_insn_done;

        // === RV64F Extension Disambiguation ===
        // Some funct7 values map to multiple operations distinguished by funct3
        // or rs2.

    L_fminmax_s:
        if (bits(insn, 14, 12) == 0b000) {
            EXEC_FMIN_S();
            goto L_insn_done;
        }
        EXEC_FMAX_S();
        goto L_insn_done;

    L_fminmax_d:
        if (bits(insn, 14, 12) == 0b000) {
            EXEC_FMIN_D();
            goto L_insn_done;
        }
        EXEC_FMAX_D();
        goto L_insn_done;

    L_fcvt_x_s: // funct7=1100000: fcvt to integer from S
        switch (rs2) {
            case 0b00000: EXEC_FCVT_W_S(); goto L_insn_done;
            case 0b00001: EXEC_FCVT_WU_S(); goto L_insn_done;
            case 0b00010: EXEC_FCVT_L_S(); goto L_insn_done;
            case 0b00011: EXEC_FCVT_LU_S(); goto L_insn_done;
            default: goto L_illegal;
        }

    L_fcvt_x_d: // funct7=1100001: fcvt to integer from D
        switch (rs2) {
            case 0b00000: EXEC_FCVT_W_D(); goto L_insn_done;
            case 0b00001: EXEC_FCVT_WU_D(); goto L_insn_done;
            case 0b00010: EXEC_FCVT_L_D(); goto L_insn_done;
            case 0b00011: EXEC_FCVT_LU_D(); goto L_insn_done;
            default: goto L_illegal;
        }

    L_fcvt_s_x: // funct7=1101000: fcvt from integer to S
        switch (rs2) {
            case 0b00000: EXEC_FCVT_S_W(); goto L_insn_done;
            case 0b00001: EXEC_FCVT_S_WU(); goto L_insn_done;
            case 0b00010: EXEC_FCVT_S_L(); goto L_insn_done;
            case 0b00011: EXEC_FCVT_S_LU(); goto L_insn_done;
            default: goto L_illegal;
        }

    L_fcvt_d_x: // funct7=1101001: fcvt from integer to D
        switch (rs2) {
            case 0b00000: EXEC_FCVT_D_W(); goto L_insn_done;
            case 0b00001: EXEC_FCVT_D_WU(); goto L_insn_done;
            case 0b00010: EXEC_FCVT_D_L(); goto L_insn_done;
            case 0b00011: EXEC_FCVT_D_LU(); goto L_insn_done;
            default: goto L_illegal;
        }

    L_fmv_x_fclass_s: // funct7=1110000
        if (bits(insn, 14, 12) == 0b000) {
            EXEC_FMV_X_W();
            goto L_insn_done;
        }
        EXEC_FCLASS_S();
        goto L_insn_done;

    L_fmv_x_fclass_d: // funct7=1110001
        if (bits(insn, 14, 12) == 0b000) {
            EXEC_FMV_X_D();
            goto L_insn_done;
        }
        EXEC_FCLASS_D();
        goto L_insn_done;

    L_fcmp_s: // funct7=1010000
        switch (bits(insn, 14, 12)) {
            case 0b010: EXEC_FEQ_S(); goto L_insn_done;
            case 0b001: EXEC_FLT_S(); goto L_insn_done;
            case 0b000: EXEC_FLE_S(); goto L_insn_done;
            default: goto L_illegal;
        }

    L_fcmp_d: // funct7=1010001
        switch (bits(insn, 14, 12)) {
            case 0b010: EXEC_FEQ_D(); goto L_insn_done;
            case 0b001: EXEC_FLT_D(); goto L_insn_done;
            case 0b000: EXEC_FLE_D(); goto L_insn_done;
            default: goto L_illegal;
        }

        // === RV64F Individual Labels (directly dispatched) ===

    L_fadd_s:
        EXEC_FADD_S();
        goto L_insn_done;
    L_fsub_s:
        EXEC_FSUB_S();
        goto L_insn_done;
    L_fmul_s:
        EXEC_FMUL_S();
        goto L_insn_done;
    L_fdiv_s:
        EXEC_FDIV_S();
        goto L_insn_done;
    L_fsqrt_s:
        EXEC_FSQRT_S();
        goto L_insn_done;
    L_fsgnj_s:
        switch (bits(insn, 14, 12)) {
            case 0b000: EXEC_FSGNJ_S(); goto L_insn_done;
            case 0b001: EXEC_FSGNJN_S(); goto L_insn_done;
            default: EXEC_FSGNJX_S(); goto L_insn_done;
        }
    L_fcvt_s_d:
        EXEC_FCVT_S_D();
        goto L_insn_done;
    L_fmv_w_x:
        EXEC_FMV_W_X();
        goto L_insn_done;

        // === RV64D Individual Labels (directly dispatched) ===

    L_fadd_d:
        EXEC_FADD_D();
        goto L_insn_done;
    L_fsub_d:
        EXEC_FSUB_D();
        goto L_insn_done;
    L_fmul_d:
        EXEC_FMUL_D();
        goto L_insn_done;
    L_fdiv_d:
        EXEC_FDIV_D();
        goto L_insn_done;
    L_fsqrt_d:
        EXEC_FSQRT_D();
        goto L_insn_done;
    L_fsgnj_d:
        switch (bits(insn, 14, 12)) {
            case 0b000: EXEC_FSGNJ_D(); goto L_insn_done;
            case 0b001: EXEC_FSGNJN_D(); goto L_insn_done;
            default: EXEC_FSGNJX_D(); goto L_insn_done;
        }
    L_fcvt_d_s:
        EXEC_FCVT_D_S();
        goto L_insn_done;
    L_fmv_d_x:
        EXEC_FMV_D_X();
        goto L_insn_done;

        // === Fused Multiply-Add Labels (opcodes
        // 1000011/1000111/1001011/1001111) ===

    L_fmadd_s:
        EXEC_FMADD_S();
        goto L_insn_done;
    L_fmadd_d:
        EXEC_FMADD_D();
        goto L_insn_done;
    L_fmsub_s:
        EXEC_FMSUB_S();
        goto L_insn_done;
    L_fmsub_d:
        EXEC_FMSUB_D();
        goto L_insn_done;
    L_fnmsub_s:
        EXEC_FNMSUB_S();
        goto L_insn_done;
    L_fnmsub_d:
        EXEC_FNMSUB_D();
        goto L_insn_done;
    L_fnmadd_s:
        EXEC_FNMADD_S();
        goto L_insn_done;
    L_fnmadd_d:
        EXEC_FNMADD_D();
        goto L_insn_done;

        // === FP Load/Store Labels ===

    L_flw:
        EXEC_FLW();
        goto L_insn_done;
    L_fld:
        EXEC_FLD();
        goto L_insn_done;
    L_fsw:
        EXEC_FSW();
        goto L_insn_done;
    L_fsd:
        EXEC_FSD();
        goto L_insn_done;

    L_insn_done:
        minstret_->advance();
        goto L_reentry;

    L_illegal:
        core::Trap::raise_exception(pc, core::TrapCause::IllegalInstruction,
                                    insn);

    } catch (const core::Trap& trap) {
        hart_->handle_trap(trap);
        goto L_reentry;
    } catch (const core::WfiWait&) {
        minstret_->advance();
        while (true) {
            if (shutdown_from_guest_) [[unlikely]]
                goto L_done;
            if (shutdown_from_host_.load(std::memory_order_relaxed))
                [[unlikely]]
                goto L_done;
            std::this_thread::yield();
            if (hart_->has_pending_enabled_interrupt()) {
                try {
                    hart_->check_interrupts();
                } catch (const core::Trap& trap) {
                    hart_->handle_trap(trap);
                    break;
                }
                break;
            }
        }
        goto L_reentry;
    } catch (...) {
        cpu_thread_exception_ = std::current_exception();
        shutdown_from_guest_ = true;
    }

L_done: {
    std::lock_guard<std::mutex> lock(cpu_mutex_);
    cpu_thread_running_ = false;
}
    cpu_cond_.notify_all();
}

} // namespace uemu

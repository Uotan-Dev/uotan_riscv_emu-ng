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

#pragma once

#include <exception>

#include "core/mmu.hpp"

// RV64I Base Integer Instructions
#define RV64I_INSTRUCTIONS(f)                                                  \
    f(add) f(addi) f(addiw) f(addw) f(and) f(andi) f(auipc) f(beq) f(bge)      \
        f(bgeu) f(blt) f(bltu) f(bne) f(fence) f(fence_i) f(jal) f(jalr) f(lb) \
            f(lbu) f(ld) f(lh) f(lhu) f(lui) f(lw) f(lwu) f(or) f(ori) f(sb)   \
                f(sd) f(sh) f(sw) f(sll) f(slli) f(slliw) f(sllw) f(slt)       \
                    f(slti) f(sltiu) f(sltu) f(sra) f(srai) f(sraiw) f(sraw)   \
                        f(srl) f(srli) f(srliw) f(srlw) f(sub) f(subw) f(xor)  \
                            f(xori)

// Zicsr Extension (CSR Instructions)
#define ZICSR_INSTRUCTIONS(f)                                                  \
    f(csrrc) f(csrrci) f(csrrs) f(csrrsi) f(csrrw) f(csrrwi)

// Privileged Instructions
#define PRIVILEGED_INSTRUCTIONS(f)                                             \
    f(ebreak) f(ecall) f(mret) f(sret) f(wfi) f(sfence_vma)

// RV64M Extension (Integer Multiplication and Division)
#define RV64M_INSTRUCTIONS(f)                                                  \
    f(mul) f(mulh) f(mulhsu) f(mulhu) f(mulw) f(div) f(divu) f(divuw) f(divw)  \
        f(rem) f(remu) f(remuw) f(remw)

// RV64A Extension (Atomic Instructions)
#define RV64A_INSTRUCTIONS(f)                                                  \
    f(lr_d) f(lr_w) f(sc_d) f(sc_w) f(amoadd_d) f(amoadd_w) f(amoand_d)        \
        f(amoand_w) f(amoor_d) f(amoor_w) f(amoxor_d) f(amoxor_w) f(amomax_d)  \
            f(amomax_w) f(amomaxu_d) f(amomaxu_w) f(amomin_d) f(amomin_w)      \
                f(amominu_d) f(amominu_w) f(amoswap_d) f(amoswap_w)

// RV64F Extension (Single-Precision Floating-Point)
#define RV64F_INSTRUCTIONS(f)                                                  \
    f(flw) f(fsw) f(fadd_s) f(fsub_s) f(fmul_s) f(fdiv_s) f(fsqrt_s)           \
        f(fsgnj_s) f(fsgnjn_s) f(fsgnjx_s) f(fmin_s) f(fmax_s) f(fclass_s)     \
            f(feq_s) f(flt_s) f(fle_s) f(fmadd_s) f(fmsub_s) f(fnmsub_s)       \
                f(fnmadd_s) f(fcvt_w_s) f(fcvt_wu_s) f(fcvt_l_s) f(fcvt_lu_s)  \
                    f(fcvt_s_w) f(fcvt_s_wu) f(fcvt_s_l) f(fcvt_s_lu)          \
                        f(fmv_x_w) f(fmv_w_x)

// RV64D Extension (Double-Precision Floating-Point)
#define RV64D_INSTRUCTIONS(f)                                                  \
    f(fld) f(fsd) f(fadd_d) f(fsub_d) f(fmul_d) f(fdiv_d) f(fsqrt_d)           \
        f(fsgnj_d) f(fsgnjn_d) f(fsgnjx_d) f(fmin_d) f(fmax_d) f(fclass_d)     \
            f(feq_d) f(flt_d) f(fle_d) f(fmadd_d) f(fmsub_d) f(fnmsub_d)       \
                f(fnmadd_d) f(fcvt_w_d) f(fcvt_wu_d) f(fcvt_l_d) f(fcvt_lu_d)  \
                    f(fcvt_d_w) f(fcvt_d_wu) f(fcvt_d_l) f(fcvt_d_lu)          \
                        f(fcvt_s_d) f(fcvt_d_s) f(fmv_x_d) f(fmv_d_x)

// RV64C Extension (Compressed Instructions)
#define RV64C_INSTRUCTIONS(f)                                                  \
    f(c_nop) f(c_addi) f(c_addiw) f(c_li) f(c_addi16sp) f(c_lui) f(c_srli)     \
        f(c_srai) f(c_andi) f(c_sub) f(c_xor) f(c_or) f(c_and) f(c_subw)       \
            f(c_addw) f(c_j) f(c_beqz) f(c_bnez) f(c_addi4spn) f(c_fld)        \
                f(c_lw) f(c_ld) f(c_fsd) f(c_sw) f(c_sd) f(c_slli) f(c_fldsp)  \
                    f(c_lwsp) f(c_ldsp) f(c_jr) f(c_mv) f(c_ebreak) f(c_jalr)  \
                        f(c_add) f(c_fsdsp) f(c_swsp) f(c_sdsp)

// Invalid/Unknown Instructions
#define INVALID_INSTRUCTIONS(f) f(c_inv) f(inv)

#define RISCV_INSTRUCTIONS(f)                                                  \
    RV64I_INSTRUCTIONS(f)                                                      \
    ZICSR_INSTRUCTIONS(f)                                                      \
    PRIVILEGED_INSTRUCTIONS(f)                                                 \
    RV64M_INSTRUCTIONS(f)                                                      \
    /* RV64A_INSTRUCTIONS(f) */                                                \
    /* RV64F_INSTRUCTIONS(f) */                                                \
    /* RV64D_INSTRUCTIONS(f) */                                                \
    /* RV64C_INSTRUCTIONS(f) */                                                \
    INVALID_INSTRUCTIONS(f)

namespace uemu::core {

#define macro(name) rv_##name,

enum class Iname : uint16_t { RISCV_INSTRUCTIONS(macro) };

#undef macro

class DecodedInsn;
using ExecFunc = void (*)(Hart* hart, MMU* mmu, const DecodedInsn* d);

enum class Itype : uint8_t {
    // 32 bit
    TYPE_I,
    TYPE_U,
    TYPE_S,
    TYPE_J,
    TYPE_R,
    TYPE_B,
    TYPE_R4,

    // 16 bit
    TYPE_CR,
    TYPE_CI,
    TYPE_CSS,
    TYPE_CIW,
    TYPE_CL,
    TYPE_CS,
    TYPE_CA,
    TYPE_CB,
    TYPE_CJ,

    TYPE_N, // none
};

enum class Ilen : uint8_t {
    Compressed = 2,
    Normal = 4,
};

class Decoder;

class DecodedInsn {
public:
    void operator()(Hart& hart, MMU& mmu) const {
        if (!exec) [[unlikely]]
            std::terminate();

        exec(&hart, &mmu, this); // throws Trap
    }

    // raw data
    uint32_t insn;
    Ilen len = Ilen::Normal;

    // name
    Iname iname = Iname::rv_inv;

    // type
    Itype type = Itype::TYPE_N;

    ExecFunc exec = nullptr;

    // decoded fields
    int8_t rd, rs1, rs2, rs3;
    uint64_t imm;

    // address of the decoded instruction
    addr_t pc;
};

class Decoder {
public:
    Decoder() = delete;
    ~Decoder() = delete;
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    static DecodedInsn decode(uint32_t insn, Ilen len, addr_t pc);

private:
    static void decode_operand(DecodedInsn& s);
};

} // namespace uemu::core

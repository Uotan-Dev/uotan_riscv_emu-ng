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

#include <utility>

#include "common/bit.hpp"
#include "common/float.hpp"
#include "core/execute.hpp" // IWYU pragma: keep

namespace uemu::core {

#define EXTRACT_OPRAND()                                                       \
    [[maybe_unused]] const auto rd = d->rd, rs1 = d->rs1, rs2 = d->rs2,        \
                                rs3 = d->rs3;                                  \
    [[maybe_unused]] const auto imm = d->imm;                                  \
    [[maybe_unused]] const size_t csr = imm & 0xFFF;                           \
    [[maybe_unused]] const auto pc = d->pc;                                    \
    [[maybe_unused]] auto& R = hart->gprs;                                     \
    [[maybe_unused]] auto& F = hart->fprs;                                     \
    [[maybe_unused]] auto& csrs = hart->csrs;

#define IMPL(insn_name, execute_process)                                       \
    void exec_##insn_name([[maybe_unused]] Hart* hart,                         \
                          [[maybe_unused]] MMU* mmu,                           \
                          [[maybe_unused]] const DecodedInsn* d) {             \
        EXTRACT_OPRAND();                                                      \
        execute_process;                                                       \
    }

namespace {

inline void fp_inst_prep(Hart* hart, const DecodedInsn* d) {
    assert(softfloat_exceptionFlags == 0);

    if (!(hart->csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::Field::FS))
        [[unlikely]]
        Trap::raise_exception(d->pc, TrapCause::IllegalInstruction, d->insn);
}

inline void fp_setup_rm(Hart* hart, const DecodedInsn* d) {
    auto rm = bits(d->insn, 14, 12);

    if (rm == FRM::RoundingMode::DYN)
        rm = hart->csrs[FRM::ADDRESS]->read_unchecked();

    if (rm > FRM::RoundingMode::RMM) [[unlikely]]
        Trap::raise_exception(d->pc, TrapCause::IllegalInstruction, d->insn);
    else
        softfloat_roundingMode = rm;
}

inline void fp_set_dirty([[maybe_unused]] Hart* hart) {
    CSR* mstatus = hart->csrs[MSTATUS::ADDRESS].get();
    reg_t v = mstatus->read_unchecked();
    mstatus->write_unchecked(v | MSTATUS::Field::FS);
}

inline void fp_update_exception_flags(Hart* hart) {
    if (softfloat_exceptionFlags) {
        fp_set_dirty(hart);

        CSR* fflags = hart->csrs[FFLAGS::ADDRESS].get();
        reg_t v = fflags->read_unchecked();
        fflags->write_unchecked(v | softfloat_exceptionFlags);

        softfloat_exceptionFlags = 0;
    }
}

inline void fp_inst_end(Hart* hart) {
    fp_set_dirty(hart);
    fp_update_exception_flags(hart);
}

} // namespace

IMPL(inv, Trap::raise_exception(pc, TrapCause::IllegalInstruction, d->insn));
IMPL(c_inv, exec_inv(hart, mmu, d));

// RV64I Base Integer Instructions
IMPL(add, R.write(rd, R[rs1] + R[rs2]))
IMPL(addi, R.write(rd, R[rs1] + imm))
IMPL(addiw, R.write(rd, sext(bits(R[rs1] + imm, 31, 0), 32)))
IMPL(addw, R.write(rd, sext(bits(R[rs1] + R[rs2], 31, 0), 32)))
IMPL(and, R.write(rd, R[rs1] & R[rs2]))
IMPL(andi, R.write(rd, R[rs1] & imm))
IMPL(auipc, R.write(rd, pc + imm))
IMPL(beq, {
    if (R[rs1] == R[rs2]) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(bge, {
    if (static_cast<int64_t>(R[rs1]) >= static_cast<int64_t>(R[rs2])) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(bgeu, {
    if (R[rs1] >= R[rs2]) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(blt, {
    if (static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(R[rs2])) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(bltu, {
    if (R[rs1] < R[rs2]) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(bne, {
    if (R[rs1] != R[rs2]) {
        addr_t npc = pc + imm;
        if (npc & 0x3) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned,
                                  npc);
        hart->pc = npc;
    }
})
IMPL(fence, /* nop */)
IMPL(fence_i, /* nop */)
IMPL(jal, {
    addr_t npc = pc + imm;
    if (npc & 0x3) [[unlikely]]
        Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned, npc);
    R.write(rd, pc + 4);
    hart->pc = npc;
})
IMPL(jalr, {
    uint64_t t = pc + 4;
    addr_t npc = (R[rs1] + imm) & ~1ULL;
    if (npc & 0x3) [[unlikely]]
        Trap::raise_exception(pc, TrapCause::InstructionAddressMisaligned, npc);
    hart->pc = npc;
    R.write(rd, t);
})
IMPL(lb, {
    uint8_t v = mmu->read<uint8_t>(pc, R[rs1] + imm);
    R.write(rd, sext(v, 8));
})
IMPL(lbu, {
    uint8_t v = mmu->read<uint8_t>(pc, R[rs1] + imm);
    R.write(rd, v);
})
IMPL(ld, {
    uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);
    R.write(rd, v);
})
IMPL(lh, {
    uint16_t v = mmu->read<uint16_t>(pc, R[rs1] + imm);
    R.write(rd, sext(v, 16));
})
IMPL(lhu, {
    uint16_t v = mmu->read<uint16_t>(pc, R[rs1] + imm);
    R.write(rd, v);
})
IMPL(lui, R.write(rd, sext(bits(imm, 31, 12) << 12, 32)))
IMPL(lw, {
    uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);
    R.write(rd, sext(v, 32));
})
IMPL(lwu, {
    uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);
    R.write(rd, v);
})
IMPL(or, R.write(rd, R[rs1] | R[rs2]))
IMPL(ori, R.write(rd, R[rs1] | imm))
IMPL(sb, mmu->write<uint8_t>(pc, R[rs1] + imm, R[rs2]))
IMPL(sd, mmu->write<uint64_t>(pc, R[rs1] + imm, R[rs2]))
IMPL(sh, mmu->write<uint16_t>(pc, R[rs1] + imm, R[rs2]))
IMPL(sll, R.write(rd, R[rs1] << bits(R[rs2], 5, 0)))
IMPL(slli, R.write(rd, R[rs1] << bits(imm, 5, 0)))
IMPL(slliw, R.write(rd, sext(bits(R[rs1], 31, 0) << bits(imm, 4, 0), 32)))
IMPL(sllw, R.write(rd, sext(static_cast<uint32_t>(bits(R[rs1], 31, 0))
                                << bits(R[rs2], 4, 0),
                            32)))
IMPL(slt,
     R.write(rd, static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(R[rs2])))
IMPL(slti,
     R.write(rd, static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(imm)))
IMPL(sltiu, R.write(rd, R[rs1] < imm))
IMPL(sltu, R.write(rd, R[rs1] < R[rs2]))
IMPL(sra, R.write(rd, static_cast<int64_t>(R[rs1]) >> bits(R[rs2], 5, 0)))
IMPL(srai, R.write(rd, static_cast<int64_t>(R[rs1]) >> bits(imm, 5, 0)))
IMPL(sraiw, R.write(rd, sext(static_cast<int32_t>(bits(R[rs1], 31, 0)) >>
                                 bits(imm, 4, 0),
                             32)))
IMPL(sraw, R.write(rd, sext(static_cast<int32_t>(bits(R[rs1], 31, 0)) >>
                                bits(R[rs2], 4, 0),
                            32)))
IMPL(srl, R.write(rd, R[rs1] >> bits(R[rs2], 5, 0)))
IMPL(srli, R.write(rd, R[rs1] >> bits(imm, 5, 0)))
IMPL(srliw, R.write(rd, sext(bits(R[rs1], 31, 0) >> bits(imm, 4, 0), 32)))
IMPL(srlw, R.write(rd, sext(bits(R[rs1], 31, 0) >> bits(R[rs2], 4, 0), 32)))
IMPL(sub, R.write(rd, R[rs1] - R[rs2]))
IMPL(subw, R.write(rd, sext(bits(R[rs1] - R[rs2], 31, 0), 32)))
IMPL(sw, mmu->write<uint32_t>(pc, R[rs1] + imm, bits(R[rs2], 31, 0)))
IMPL(xor, R.write(rd, R[rs1] ^ R[rs2]))
IMPL(xori, R.write(rd, R[rs1] ^ imm))

// Zicsr Extension (CSR Instructions) and Privileged Instructions
IMPL(csrrc, {
    reg_t t = csrs[csr]->read_checked(*d);
    if (rs1)
        csrs[csr]->write_checked(*d, t & ~R[rs1]);
    R.write(rd, t);
})
IMPL(csrrci, {
    uint64_t zimm = bits(d->insn, 19, 15);
    reg_t t = csrs[csr]->read_checked(*d);
    if (zimm)
        csrs[csr]->write_checked(*d, t & ~zimm);
    R.write(rd, t);
})
IMPL(csrrs, {
    uint64_t t = csrs[csr]->read_checked(*d);
    if (rs1)
        csrs[csr]->write_checked(*d, t | R[rs1]);
    R.write(rd, t);
})
IMPL(csrrsi, {
    uint64_t zimm = bits(d->insn, 19, 15);
    uint64_t t = csrs[csr]->read_checked(*d);
    if (zimm)
        csrs[csr]->write_checked(*d, t | zimm);
    R.write(rd, t);
})
IMPL(csrrw, {
    if (rd) {
        uint64_t t = csrs[csr]->read_checked(*d);
        csrs[csr]->write_checked(*d, R[rs1]);
        R.write(rd, t);
    } else {
        csrs[csr]->write_checked(*d, R[rs1]);
    }
})
IMPL(csrrwi, {
    uint64_t zimm = bits(d->insn, 19, 15);
    if (rd)
        R.write(rd, csrs[csr]->read_checked(*d));
    csrs[csr]->write_checked(*d, zimm);
})
IMPL(ebreak, Trap::raise_exception(pc, TrapCause::Breakpoint, pc))
IMPL(ecall, {
    switch (hart->priv) {
        case PrivilegeLevel::M:
            Trap::raise_exception(pc, TrapCause::EnvironmentCallFromM, 0);
            break;
        case PrivilegeLevel::S:
            Trap::raise_exception(pc, TrapCause::EnvironmentCallFromS, 0);
            break;
        case PrivilegeLevel::U:
            Trap::raise_exception(pc, TrapCause::EnvironmentCallFromU, 0);
            break;
        default: std::unreachable();
    }
})
IMPL(mret, {
    if (hart->priv != PrivilegeLevel::M) [[unlikely]]
        Trap::raise_exception(pc, TrapCause::IllegalInstruction, d->insn);

    reg_t mstatus = hart->csrs[MSTATUS::ADDRESS]->read_unchecked();

    hart->pc = hart->csrs[MEPC::ADDRESS]->read_unchecked();
    hart->priv = static_cast<PrivilegeLevel>((mstatus & MSTATUS::Field::MPP) >>
                                             MSTATUS::Shift::MPP_SHIFT);

    if (hart->priv != PrivilegeLevel::M)
        mstatus &= ~MSTATUS::Field::MPRV;

    if (mstatus & MSTATUS::Field::MPIE)
        mstatus |= MSTATUS::Field::MIE;
    else
        mstatus &= ~MSTATUS::Field::MIE;

    mstatus |= MSTATUS::Field::MPIE;
    mstatus &= ~MSTATUS::Field::MPP;

    hart->csrs[MSTATUS::ADDRESS]->write_unchecked(mstatus);
})
IMPL(sfence_vma, {
    if (hart->priv == PrivilegeLevel::U ||
        (hart->priv == PrivilegeLevel::S &&
         (hart->csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TVM)))
        [[unlikely]]
        Trap::raise_exception(pc, TrapCause::IllegalInstruction, d->insn);
})
IMPL(sret, {
    if (hart->priv == PrivilegeLevel::U ||
        (hart->priv == PrivilegeLevel::S &&
         (hart->csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TSR)))
        [[unlikely]]
        Trap::raise_exception(pc, TrapCause::IllegalInstruction, d->insn);

    reg_t sstatus = hart->csrs[SSTATUS::ADDRESS]->read_unchecked();

    hart->pc = hart->csrs[SEPC::ADDRESS]->read_unchecked();
    hart->priv = static_cast<PrivilegeLevel>((sstatus & SSTATUS::Field::SPP) >>
                                             SSTATUS::Shift::SPP_SHIFT);

    if (hart->priv != PrivilegeLevel::M)
        sstatus &= ~SSTATUS::Field::MPRV;

    if (sstatus & SSTATUS::Field::SPIE)
        sstatus |= SSTATUS::Field::SIE;
    else
        sstatus &= ~SSTATUS::Field::SIE;

    sstatus |= SSTATUS::Field::SPIE;
    sstatus &= ~SSTATUS::Field::SPP;

    hart->csrs[SSTATUS::ADDRESS]->write_unchecked(sstatus);
})
IMPL(wfi, {
    if (hart->priv == PrivilegeLevel::U ||
        (hart->priv < PrivilegeLevel::M &&
         (hart->csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TW)))
        Trap::raise_exception(pc, TrapCause::IllegalInstruction, d->insn);
})

// RV64M Extension
IMPL(div, {
    if (static_cast<int64_t>(R[rs2]) == 0) [[unlikely]]
        R.write(rd, ~0ULL);
    else if (static_cast<int64_t>(R[rs1]) == INT64_MIN &&
             static_cast<int64_t>(R[rs2]) == -1) [[unlikely]]
        R.write(rd, static_cast<int64_t>(R[rs1]));
    else
        R.write(rd,
                static_cast<int64_t>(R[rs1]) / static_cast<int64_t>(R[rs2]));
})
IMPL(divu, {
    if (R[rs2] == 0) [[unlikely]]
        R.write(rd, ~0ULL);
    else
        R.write(rd, R[rs1] / R[rs2]);
})
IMPL(divuw, {
    uint32_t v1 = bits(R[rs1], 31, 0);
    uint32_t v2 = bits(R[rs2], 31, 0);
    if (v2 == 0) [[unlikely]]
        R.write(rd, ~0ULL);
    else
        R.write(rd, sext(v1 / v2, 32));
})
IMPL(divw, {
    int32_t v1 = static_cast<int32_t>(bits(R[rs1], 31, 0));
    int32_t v2 = static_cast<int32_t>(bits(R[rs2], 31, 0));
    if (v2 == 0) [[unlikely]]
        R.write(rd, ~0ULL);
    else if (v1 == INT32_MIN && v2 == -1) [[unlikely]]
        R.write(rd, sext(v1, 32));
    else
        R.write(rd, sext(v1 / v2, 32));
})
IMPL(mul, R.write(rd, R[rs1] * R[rs2]))
IMPL(mulh, R.write(rd, (int64_t)(((__int128_t)(int64_t)R[rs1] *
                                  (__int128_t)(int64_t)R[rs2]) >>
                                 64)))
IMPL(mulhsu, R.write(rd, (int64_t)(((__int128_t)(int64_t)R[rs1] *
                                    (__uint128_t)R[rs2]) >>
                                   64)))
IMPL(mulhu,
     R.write(rd, (uint64_t)((__uint128_t)R[rs1] * (__uint128_t)R[rs2] >> 64)))
IMPL(mulw, R.write(rd, sext(bits(R[rs1] * R[rs2], 31, 0), 32)))
IMPL(rem, {
    if (static_cast<int64_t>(R[rs2]) == 0) [[unlikely]]
        R.write(rd, (int64_t)R[rs1]);
    else if (static_cast<int64_t>(R[rs1]) == INT64_MIN &&
             static_cast<int64_t>(R[rs2]) == -1) [[unlikely]]
        R.write(rd, 0);
    else
        R.write(rd,
                static_cast<int64_t>(R[rs1]) % static_cast<int64_t>(R[rs2]));
})
IMPL(remu, {
    if (R[rs2] == 0) [[unlikely]]
        R.write(rd, R[rs1]);
    else
        R.write(rd, R[rs1] % R[rs2]);
})
IMPL(remuw, {
    uint32_t v1 = bits(R[rs1], 31, 0);
    uint32_t v2 = bits(R[rs2], 31, 0);
    if (v2 == 0) [[unlikely]]
        R.write(rd, sext(v1, 32));
    else
        R.write(rd, sext(v1 % v2, 32));
})
IMPL(remw, {
    int32_t v1 = (int32_t)bits(R[rs1], 31, 0);
    int32_t v2 = (int32_t)bits(R[rs2], 31, 0);
    if (v2 == 0) [[unlikely]]
        R.write(rd, sext(v1, 32));
    else if (v1 == INT32_MIN && v2 == -1) [[unlikely]]
        R.write(rd, 0);
    else
        R.write(rd, sext(v1 % v2, 32));
})

// RV64A Extension
IMPL(lr_d, {
    uint64_t v = mmu->read<uint64_t>(pc, R[rs1]);
    R.write(rd, v);
    mmu->reservation_address = R[rs1];
    mmu->reservation_valid = true;
})
IMPL(lr_w, {
    uint32_t v = mmu->read<uint32_t>(pc, R[rs1]);
    R.write(rd, sext(v, 32));
    mmu->reservation_address = R[rs1];
    mmu->reservation_valid = true;
})
IMPL(sc_d, {
    if (mmu->reservation_valid && mmu->reservation_address == R[rs1]) {
        mmu->write<uint64_t>(pc, R[rs1], R[rs2]);
        R.write(rd, 0);
    } else {
        R.write(rd, 1);
    }
    mmu->reservation_valid = false;
})
IMPL(sc_w, {
    if (mmu->reservation_valid && mmu->reservation_address == R[rs1]) {
        mmu->write<uint32_t>(pc, R[rs1], R[rs2]);
        R.write(rd, 0);
    } else {
        R.write(rd, 1);
    }
    mmu->reservation_valid = false;
})
IMPL(amoadd_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1], static_cast<int64_t>(t) + static_cast<int64_t>(R[rs2]));
    R.write(rd, t);
})
IMPL(amoadd_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1], static_cast<int32_t>(t) + static_cast<int32_t>(R[rs2]));
    R.write(rd, sext(t, 32));
})
IMPL(amoand_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1], static_cast<int64_t>(t) & static_cast<int64_t>(R[rs2]));
    R.write(rd, t);
})
IMPL(amoand_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1], static_cast<int32_t>(t) & static_cast<int32_t>(R[rs2]));
    R.write(rd, sext(t, 32));
})
IMPL(amoor_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1], static_cast<int64_t>(t) | static_cast<int64_t>(R[rs2]));
    R.write(rd, t);
})
IMPL(amoor_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1], static_cast<int32_t>(t) | static_cast<int32_t>(R[rs2]));
    R.write(rd, sext(t, 32));
})
IMPL(amoxor_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1], static_cast<int64_t>(t) ^ static_cast<int64_t>(R[rs2]));
    R.write(rd, t);
})
IMPL(amoxor_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1], static_cast<int32_t>(t) ^ static_cast<int32_t>(R[rs2]));
    R.write(rd, sext(t, 32));
})
IMPL(amomax_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1],
        std::max(static_cast<int64_t>(t), static_cast<int64_t>(R[rs2])));
    R.write(rd, t);
})
IMPL(amomax_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1],
        std::max(static_cast<int32_t>(t), static_cast<int32_t>(R[rs2])));
    R.write(rd, sext(t, 32));
})
IMPL(amomaxu_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(pc, R[rs1], std::max(t, R[rs2]));
    R.write(rd, t);
})
IMPL(amomaxu_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1],
        std::max(static_cast<uint32_t>(t), static_cast<uint32_t>(R[rs2])));
    R.write(rd, sext(t, 32));
})
IMPL(amomin_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(
        pc, R[rs1],
        std::min(static_cast<int64_t>(t), static_cast<int64_t>(R[rs2])));
    R.write(rd, t);
})
IMPL(amomin_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1],
        std::min(static_cast<int32_t>(t), static_cast<int32_t>(R[rs2])));
    R.write(rd, sext(t, 32));
})
IMPL(amominu_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(pc, R[rs1], std::min(t, R[rs2]));
    R.write(rd, t);
})
IMPL(amominu_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(
        pc, R[rs1],
        std::min(static_cast<uint32_t>(t), static_cast<uint32_t>(R[rs2])));
    R.write(rd, sext(t, 32));
})
IMPL(amoswap_d, {
    uint64_t t = mmu->read<uint64_t>(pc, R[rs1]);
    mmu->write<uint64_t>(pc, R[rs1], R[rs2]);
    R.write(rd, t);
})
IMPL(amoswap_w, {
    uint32_t t = mmu->read<uint32_t>(pc, R[rs1]);
    mmu->write<uint32_t>(pc, R[rs1], R[rs2]);
    R.write(rd, sext(t, 32));
})

// RV64F Extension
IMPL(flw, {
    fp_inst_prep(hart, d);
    uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);
    F[rd] = float32_t{v};
})
IMPL(fsw, {
    fp_inst_prep(hart, d);
    mmu->write<uint32_t>(pc, R[rs1] + imm,
                         static_cast<uint32_t>(F[rs2].read_64().v));
})
IMPL(fadd_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_add(F[rs1].read_32(), F[rs2].read_32());
    fp_inst_end(hart);
})
IMPL(fsub_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_sub(F[rs1].read_32(), F[rs2].read_32());
    fp_inst_end(hart);
})
IMPL(fmul_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_mul(F[rs1].read_32(), F[rs2].read_32());
    fp_inst_end(hart);
})
IMPL(fdiv_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_div(F[rs1].read_32(), F[rs2].read_32());
    fp_inst_end(hart);
})
IMPL(fsqrt_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_sqrt(F[rs1].read_32());
    fp_inst_end(hart);
})
IMPL(fsgnj_s, {
    fp_inst_prep(hart, d);
    float32_t f1 = F[rs1].read_32();
    float32_t f2 = F[rs2].read_32();
    F[rd] = float32_t{(f1.v & ~F32_SIGN) | (f2.v & F32_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fsgnjn_s, {
    fp_inst_prep(hart, d);
    float32_t f1 = F[rs1].read_32();
    float32_t f2 = F[rs2].read_32();
    F[rd] = float32_t{(f1.v & ~F32_SIGN) | (~f2.v & F32_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fsgnjx_s, {
    fp_inst_prep(hart, d);
    float32_t f1 = F[rs1].read_32();
    float32_t f2 = F[rs2].read_32();
    F[rd] = float32_t{f1.v ^ (f2.v & F32_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fmin_s, {
    fp_inst_prep(hart, d);
    float32_t f1 = F[rs1].read_32();
    float32_t f2 = F[rs2].read_32();

    if (f32_isSignalingNaN(f1) || f32_isSignalingNaN(f2)) {
        CSR* fflags = csrs[FFLAGS::ADDRESS].get();
        reg_t v = fflags->read_unchecked();
        fflags->write_unchecked(v | FFLAGS::Field::NV);
    }

    bool smaller =
        f32_lt_quiet(f1, f2) || (f32_eq(f1, f2) && f32_isNegative(f1));

    if (f32_isNaN(f1) && f32_isNaN(f2)) {
        F[rd] = float32_t{F32_DEFAULT_NAN};
    } else {
        if (smaller || f32_isNaN(f2))
            F[rd] = f1;
        else
            F[rd] = f2;
    }
    fp_inst_end(hart);
})
IMPL(fmax_s, {
    fp_inst_prep(hart, d);
    float32_t f1 = F[rs1].read_32();
    float32_t f2 = F[rs2].read_32();

    if (f32_isSignalingNaN(f1) || f32_isSignalingNaN(f2)) {
        CSR* fflags = csrs[FFLAGS::ADDRESS].get();
        reg_t v = fflags->read_unchecked();
        fflags->write_unchecked(v | FFLAGS::Field::NV);
    }

    bool greater =
        f32_lt_quiet(f2, f1) || (f32_eq(f2, f1) && f32_isNegative(f2));

    if (f32_isNaN(f1) && f32_isNaN(f2)) {
        F[rd] = float32_t{F32_DEFAULT_NAN};
    } else {
        if (greater || f32_isNaN(f2))
            F[rd] = f1;
        else
            F[rd] = f2;
    }
    fp_inst_end(hart);
})
IMPL(fcvt_w_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, static_cast<int64_t>(f32_to_i32(F[rs1].read_32(),
                                                softfloat_roundingMode, true)));
    fp_inst_end(hart);
})
IMPL(fcvt_wu_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, static_cast<int64_t>(static_cast<int32_t>(f32_to_ui32(
                    F[rs1].read_32(), softfloat_roundingMode, true))));
    fp_inst_end(hart);
})
IMPL(fcvt_l_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, f32_to_i64(F[rs1].read_32(), softfloat_roundingMode, true));
    fp_inst_end(hart);
})
IMPL(fcvt_lu_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, f32_to_ui64(F[rs1].read_32(), softfloat_roundingMode, true));
    fp_inst_end(hart);
})
IMPL(fcvt_s_w, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = i32_to_f32(static_cast<int32_t>(R[rs1]));
    fp_inst_end(hart);
})
IMPL(fcvt_s_wu, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = ui32_to_f32(static_cast<uint32_t>(R[rs1]));
    fp_inst_end(hart);
})
IMPL(fcvt_s_l, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = i64_to_f32(R[rs1]);
    fp_inst_end(hart);
})
IMPL(fcvt_s_lu, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = ui64_to_f32(R[rs1]);
    fp_inst_end(hart);
})
IMPL(fmv_x_w, {
    fp_inst_prep(hart, d);
    R.write(rd, static_cast<int64_t>(static_cast<int32_t>(
                    static_cast<uint32_t>(F[rs1].read_64().v))));
})
IMPL(fmv_w_x, {
    fp_inst_prep(hart, d);
    F[rd] = float32_t{static_cast<uint32_t>(static_cast<int32_t>(R[rs1]))};
    fp_set_dirty(hart);
})
IMPL(fclass_s, {
    fp_inst_prep(hart, d);
    R.write(rd, static_cast<int64_t>(
                    static_cast<int32_t>(f32_classify(F[rs1].read_32()))));
})
IMPL(feq_s, {
    fp_inst_prep(hart, d);
    R.write(rd, f32_eq(F[rs1].read_32(), F[rs2].read_32()));
    fp_update_exception_flags(hart);
})
IMPL(flt_s, {
    fp_inst_prep(hart, d);
    R.write(rd, f32_lt(F[rs1].read_32(), F[rs2].read_32()));
    fp_update_exception_flags(hart);
})
IMPL(fle_s, {
    fp_inst_prep(hart, d);
    R.write(rd, f32_le(F[rs1].read_32(), F[rs2].read_32()));
    fp_update_exception_flags(hart);
})
IMPL(fmadd_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_mulAdd(F[rs1].read_32(), F[rs2].read_32(), F[rs3].read_32());
    fp_inst_end(hart);
})
IMPL(fmsub_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_mulAdd(F[rs1].read_32(), F[rs2].read_32(),
                       f32_neg(F[rs3].read_32()));
    fp_inst_end(hart);
})
IMPL(fnmsub_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_mulAdd(f32_neg(F[rs1].read_32()), F[rs2].read_32(),
                       F[rs3].read_32());
    fp_inst_end(hart);
})
IMPL(fnmadd_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_mulAdd(f32_neg(F[rs1].read_32()), F[rs2].read_32(),
                       f32_neg(F[rs3].read_32()));
    fp_inst_end(hart);
})

// RV64D Extension
IMPL(fld, {
    fp_inst_prep(hart, d);
    uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);
    F[rd] = float64_t{v};
})
IMPL(fsd, {
    fp_inst_prep(hart, d);
    mmu->write<uint64_t>(pc, R[rs1] + imm, F[rs2].read_64().v);
})
IMPL(fadd_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_add(F[rs1].read_64(), F[rs2].read_64());
    fp_inst_end(hart);
})
IMPL(fsub_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_sub(F[rs1].read_64(), F[rs2].read_64());
    fp_inst_end(hart);
})
IMPL(fmul_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_mul(F[rs1].read_64(), F[rs2].read_64());
    fp_inst_end(hart);
})
IMPL(fdiv_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_div(F[rs1].read_64(), F[rs2].read_64());
    fp_inst_end(hart);
})
IMPL(fsqrt_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_sqrt(F[rs1].read_64());
    fp_inst_end(hart);
})
IMPL(fsgnj_d, {
    fp_inst_prep(hart, d);
    float64_t f1 = F[rs1].read_64();
    float64_t f2 = F[rs2].read_64();
    F[rd] = float64_t{(f1.v & ~F64_SIGN) | (f2.v & F64_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fsgnjn_d, {
    fp_inst_prep(hart, d);
    float64_t f1 = F[rs1].read_64();
    float64_t f2 = F[rs2].read_64();
    F[rd] = float64_t{(f1.v & ~F64_SIGN) | (~f2.v & F64_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fsgnjx_d, {
    fp_inst_prep(hart, d);
    float64_t f1 = F[rs1].read_64();
    float64_t f2 = F[rs2].read_64();
    F[rd] = float64_t{f1.v ^ (f2.v & F64_SIGN)};
    fp_set_dirty(hart);
})
IMPL(fmin_d, {
    fp_inst_prep(hart, d);
    float64_t f1 = F[rs1].read_64();
    float64_t f2 = F[rs2].read_64();

    if (f64_isSignalingNaN(f1) || f64_isSignalingNaN(f2)) {
        CSR* fflags = csrs[FFLAGS::ADDRESS].get();
        reg_t v = fflags->read_unchecked();
        fflags->write_unchecked(v | FFLAGS::Field::NV);
    }

    bool smaller =
        f64_lt_quiet(f1, f2) || (f64_eq(f1, f2) && f64_isNegative(f1));

    if (f64_isNaN(f1) && f64_isNaN(f2)) {
        F[rd] = float64_t{F64_DEFAULT_NAN};
    } else {
        if (smaller || f64_isNaN(f2))
            F[rd] = f1;
        else
            F[rd] = f2;
    }
    fp_inst_end(hart);
})
IMPL(fmax_d, {
    fp_inst_prep(hart, d);
    float64_t f1 = F[rs1].read_64();
    float64_t f2 = F[rs2].read_64();

    if (f64_isSignalingNaN(f1) || f64_isSignalingNaN(f2)) {
        CSR* fflags = csrs[FFLAGS::ADDRESS].get();
        reg_t v = fflags->read_unchecked();
        fflags->write_unchecked(v | FFLAGS::Field::NV);
    }

    bool greater =
        f64_lt_quiet(f2, f1) || (f64_eq(f2, f1) && f64_isNegative(f2));

    if (f64_isNaN(f1) && f64_isNaN(f2)) {
        F[rd] = float64_t{F64_DEFAULT_NAN};
    } else {
        if (greater || f64_isNaN(f2))
            F[rd] = f1;
        else
            F[rd] = f2;
    }
    fp_inst_end(hart);
})
IMPL(fcvt_w_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, static_cast<int64_t>(f64_to_i32(F[rs1].read_64(),
                                                softfloat_roundingMode, true)));
    fp_inst_end(hart);
})
IMPL(fcvt_wu_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, static_cast<int64_t>(static_cast<int32_t>(f64_to_ui32(
                    F[rs1].read_64(), softfloat_roundingMode, true))));
    fp_inst_end(hart);
})
IMPL(fcvt_l_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, f64_to_i64(F[rs1].read_64(), softfloat_roundingMode, true));
    fp_inst_end(hart);
})
IMPL(fcvt_lu_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    R.write(rd, f64_to_ui64(F[rs1].read_64(), softfloat_roundingMode, true));
    fp_inst_end(hart);
})
IMPL(fcvt_d_w, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = i32_to_f64(static_cast<int32_t>(R[rs1]));
    fp_inst_end(hart);
})
IMPL(fcvt_d_wu, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = ui32_to_f64(static_cast<uint32_t>(R[rs1]));
    fp_inst_end(hart);
})
IMPL(fcvt_d_l, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = i64_to_f64(R[rs1]);
    fp_inst_end(hart);
})
IMPL(fcvt_d_lu, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = ui64_to_f64(R[rs1]);
    fp_inst_end(hart);
})
IMPL(fcvt_s_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_to_f32(F[rs1].read_64());
    fp_inst_end(hart);
})
IMPL(fcvt_d_s, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f32_to_f64(F[rs1].read_32());
    fp_inst_end(hart);
})
IMPL(fmv_x_d, {
    fp_inst_prep(hart, d);
    R.write(rd, F[rs1].read_64().v);
})
IMPL(fmv_d_x, {
    fp_inst_prep(hart, d);
    F[rd] = float64_t{R[rs1]};
    fp_set_dirty(hart);
})
IMPL(fclass_d, {
    fp_inst_prep(hart, d);
    R.write(rd, static_cast<int64_t>(f64_classify(F[rs1].read_64())));
})
IMPL(feq_d, {
    fp_inst_prep(hart, d);
    R.write(rd, f64_eq(F[rs1].read_64(), F[rs2].read_64()));
    fp_update_exception_flags(hart);
})
IMPL(flt_d, {
    fp_inst_prep(hart, d);
    R.write(rd, f64_lt(F[rs1].read_64(), F[rs2].read_64()));
    fp_update_exception_flags(hart);
})
IMPL(fle_d, {
    fp_inst_prep(hart, d);
    R.write(rd, f64_le(F[rs1].read_64(), F[rs2].read_64()));
    fp_update_exception_flags(hart);
})
IMPL(fmadd_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_mulAdd(F[rs1].read_64(), F[rs2].read_64(), F[rs3].read_64());
    fp_inst_end(hart);
})
IMPL(fmsub_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_mulAdd(F[rs1].read_64(), F[rs2].read_64(),
                       f64_neg(F[rs3].read_64()));
    fp_inst_end(hart);
})
IMPL(fnmsub_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_mulAdd(f64_neg(F[rs1].read_64()), F[rs2].read_64(),
                       F[rs3].read_64());
    fp_inst_end(hart);
})
IMPL(fnmadd_d, {
    fp_inst_prep(hart, d);
    fp_setup_rm(hart, d);
    F[rd] = f64_mulAdd(f64_neg(F[rs1].read_64()), F[rs2].read_64(),
                       f64_neg(F[rs3].read_64()));
    fp_inst_end(hart);
})

} // namespace uemu::core

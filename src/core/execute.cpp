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

#include <utility>

#include "common/bit.hpp"
#include "core/execute.hpp" // IWYU pragma: keep

namespace uemu::core {

#define EXTRACT_OPRAND()                                                       \
    [[maybe_unused]] const auto rd = d->rd, rs1 = d->rs1, rs2 = d->rs2,        \
                                rs3 = d->rs3;                                  \
    [[maybe_unused]] const auto imm = d->imm;                                  \
    [[maybe_unused]] const size_t csr = imm & 0xFFF;                           \
    [[maybe_unused]] const auto pc = d->pc;                                    \
    [[maybe_unused]] RegisterFile& R = hart->gprs;                             \
    [[maybe_unused]] auto& csrs = hart->csrs;

#define IMPL(insn_name, execute_process)                                       \
    void exec_##insn_name([[maybe_unused]] Hart* hart,                         \
                          [[maybe_unused]] MMU* mmu,                           \
                          [[maybe_unused]] const DecodedInsn* d) {             \
        EXTRACT_OPRAND();                                                      \
        execute_process;                                                       \
    }

#define unimplemented()                                                        \
    do {                                                                       \
        Trap::raise_exception(pc, Exception::IllegalInstruction, d->insn);     \
    } while (0)

IMPL(inv, Trap::raise_exception(pc, Exception::IllegalInstruction, d->insn));
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
    if (R[rs1] == R[rs2])
        hart->pc = pc + imm;
})
IMPL(bge, {
    if (static_cast<int64_t>(R[rs1]) >= static_cast<int64_t>(R[rs2]))
        hart->pc = pc + imm;
})
IMPL(bgeu, {
    if (R[rs1] >= R[rs2])
        hart->pc = pc + imm;
})
IMPL(blt, {
    if (static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(R[rs2]))
        hart->pc = pc + imm;
})
IMPL(bltu, {
    if (R[rs1] < R[rs2])
        hart->pc = pc + imm;
})
IMPL(bne, {
    if (R[rs1] != R[rs2])
        hart->pc = pc + imm;
})
IMPL(fence, /* nop */)
IMPL(fence_i, /* nop */)
IMPL(jal, {
    R.write(rd, pc + 4);
    hart->pc = pc + imm;
})
IMPL(jalr, {
    uint64_t t = pc + 4;
    hart->pc = (R[rs1] + imm) & ~1ULL;
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
    csrs[csr]->write_checked(*d, t & ~R[rs1]);
    R.write(rd, t);
})
IMPL(csrrci, {
    uint64_t zimm = bits(d->insn, 19, 15);
    reg_t t = csrs[csr]->read_checked(*d);
    csrs[csr]->write_checked(*d, t & ~zimm);
    R.write(rd, t);
})
IMPL(csrrs, {
    uint64_t t = csrs[csr]->read_checked(*d);
    csrs[csr]->write_checked(*d, t | R[rs1]);
    R.write(rd, t);
})
IMPL(csrrsi, {
    uint64_t zimm = bits(d->insn, 19, 15);
    uint64_t t = csrs[csr]->read_checked(*d);
    csrs[csr]->write_checked(*d, t | zimm);
    R.write(rd, t);
})
IMPL(csrrw, {
    uint64_t t = csrs[csr]->read_checked(*d);
    csrs[csr]->write_checked(*d, R[rs1]);
    R.write(rd, t);
})
IMPL(csrrwi, {
    uint64_t zimm = bits(d->insn, 19, 15);
    uint64_t t = csrs[csr]->read_checked(*d);
    R.write(rd, t);
    csrs[csr]->write_checked(*d, zimm);
})
IMPL(ebreak, Trap::raise_exception(pc, Exception::Breakpoint, pc))
IMPL(ecall, {
    switch (hart->priv) {
        case PrivilegeLevel::M:
            Trap::raise_exception(pc, Exception::EnvironmentCallFromM, 0);
            break;
        case PrivilegeLevel::S:
            Trap::raise_exception(pc, Exception::EnvironmentCallFromS, 0);
            break;
        case PrivilegeLevel::U:
            Trap::raise_exception(pc, Exception::EnvironmentCallFromU, 0);
            break;
        default: std::unreachable();
    }
})
IMPL(mret, {
    if (hart->priv != PrivilegeLevel::M) [[unlikely]]
        Trap::raise_exception(pc, Exception::IllegalInstruction, d->insn);

    uint64_t mstatus = hart->csrs[MSTATUS::ADDRESS]->read_unchecked();

    hart->pc = hart->csrs[MEPC::ADDRESS]->read_unchecked();
    hart->priv = static_cast<PrivilegeLevel>((mstatus & MSTATUS::field::MPP) >>
                                             MSTATUS::shift::MPP_SHIFT);

    if (hart->priv != PrivilegeLevel::M)
        mstatus &= ~MSTATUS::field::MPRV;

    if (mstatus & MSTATUS::field::MPIE)
        mstatus |= MSTATUS::field::MIE;
    else
        mstatus &= ~MSTATUS::field::MIE;

    mstatus |= MSTATUS::field::MPIE;
    mstatus &= ~MSTATUS::field::MPP;

    hart->csrs[MSTATUS::ADDRESS]->write_unchecked(mstatus);
})
IMPL(sfence_vma, unimplemented();)
IMPL(sret, unimplemented();)
IMPL(wfi, /* nop */)

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

} // namespace uemu::core

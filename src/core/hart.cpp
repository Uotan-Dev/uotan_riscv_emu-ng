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

#include <cassert>
#include <exception>
#include <iostream>

#include "core/decoder.hpp"
#include "core/hart.hpp"

namespace uemu::core {

reg_t CSR::read_checked(const DecodedInsn& insn, PrivilegeLevel priv) const {
    if (!check_permissions(priv)) [[unlikely]]
        Trap::raise_exception(insn.pc, Exception::IllegalInstruction,
                              insn.insn);

    return read_unchecked();
}

void CSR::write_checked(const DecodedInsn& insn, PrivilegeLevel priv, reg_t v) {
    if (!check_permissions(priv)) [[unlikely]]
        Trap::raise_exception(insn.pc, Exception::IllegalInstruction,
                              insn.insn);

    write_unchecked(v);
}

[[noreturn]] reg_t
UnimplementedCSR::read_checked(const DecodedInsn& insn,
                               [[maybe_unused]] PrivilegeLevel priv) const {
    if (trace_)
        std::cout << "Unimplemented CSR: " << address_ << std::endl;

    Trap::raise_exception(insn.pc, Exception::IllegalInstruction, insn.insn);
}

[[noreturn]] void
UnimplementedCSR::write_checked(const DecodedInsn& insn,
                                [[maybe_unused]] PrivilegeLevel priv,
                                [[maybe_unused]] reg_t v) {
    if (trace_)
        std::cout << "Unimplemented CSR: " << address_ << std::endl;

    Trap::raise_exception(insn.pc, Exception::IllegalInstruction, insn.insn);
}

[[noreturn]]
void ConstCSR::write_checked(const DecodedInsn& insn,
                             [[maybe_unused]] PrivilegeLevel priv,
                             [[maybe_unused]] reg_t v) {
    Trap::raise_exception(insn.pc, Exception::IllegalInstruction, insn.insn);
}

MSTATUS::MSTATUS(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
    using F = MSTATUS::field;
    using S = MSTATUS::shift;

    read_mask_ = F::SIE | F::MIE | F::SPIE | F::MPIE | F::SPP | F::MPP | F::FS |
                 F::MPRV | F::SUM | F::MXR | F::TVM | F::TW | F::TSR | F::UXL |
                 F::SXL | F::SD;

    // FIXME: MPP is excluded from write_mask_, bring it back and make it WARL
    // after S-mode implementation
    write_mask_ = F::MIE | F::MPIE | F::MPRV;

    reg_t misa = hart->csrs[MISA::ADDRESS]->read_unchecked();
    reg_t mxl = (misa >> MISA::MXL_SHIFT) & 0x3;
    assert(mxl == MISA::xlen_32 || mxl == MISA::xlen_64);

    if (misa & MISA::S) {
        write_mask_ |= F::SIE | F::SPIE | F::SPP | F::SUM | F::MXR | F::TVM |
                       F::TW | F::TSR;

        value_ |= (mxl << S::SXL_SHIFT);
    }

    if (misa & MISA::U)
        value_ |= (mxl << S::UXL_SHIFT);

    if (misa & (MISA::F | MISA::D))
        value_ |= F::FS | F::SD;

    value_ |= static_cast<reg_t>(PrivilegeLevel::M) << S::MPP_SHIFT;
}

Hart::Hart(addr_t reset_pc) : pc(reset_pc) {
    csrs[MISA::ADDRESS] = std::make_shared<MISA>(
        this, MISA::field::I | MISA::field::M |
                  (MISA::Mxl::xlen_64 << MISA::shift::MXL_SHIFT));
    csrs[MVENDORID::ADDRESS] = std::make_shared<MVENDORID>(this, 0);
    csrs[MARCHID::ADDRESS] = std::make_shared<MARCHID>(this, 0);
    csrs[MIMPID::ADDRESS] = std::make_shared<MIMPID>(this, 0x00000010);
    csrs[MHARTID::ADDRESS] = std::make_shared<MHARTID>(this, 0);
    csrs[MSTATUS::ADDRESS] = std::make_shared<MSTATUS>(this);
    csrs[MTVEC::ADDRESS] = std::make_shared<MTVEC>(this);
    csrs[MSCRATCH::ADDRESS] = std::make_shared<MSCRATCH>(this);
    csrs[MEPC::ADDRESS] = std::make_shared<MEPC>(this);
    csrs[MCAUSE::ADDRESS] = std::make_shared<MCAUSE>(this);
    csrs[MTVAL::ADDRESS] = std::make_shared<MTVAL>(this);
    csrs[MCONFIGPTR::ADDRESS] = std::make_shared<MCONFIGPTR>(this);
    csrs[MCYCLE::ADDRESS] = std::make_shared<MCYCLE>(this);
    csrs[MINSTRET::ADDRESS] = std::make_shared<MINSTRET>(this);

    for (size_t i = MHPMCOUNTERN::MIN_ADDRESS; i <= MHPMCOUNTERN::MAX_ADDRESS;
         i += MHPMCOUNTERN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<MHPMCOUNTERN>(this);

    for (size_t i = MHPMEVENTN::MIN_ADDRESS; i <= MHPMEVENTN::MAX_ADDRESS;
         i += MHPMEVENTN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<MHPMEVENTN>(this);

    for (size_t i = PMPCFGN::MIN_ADDRESS; i <= PMPCFGN::MAX_ADDRESS;
         i += PMPCFGN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<PMPCFGN>(this);

    for (size_t i = PMPADDRN::MIN_ADDRESS; i <= PMPADDRN::MAX_ADDRESS;
         i += PMPADDRN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<PMPADDRN>(this);

    csrs[TSELECT::ADDRESS] = std::make_shared<TSELECT>(this);

    for (size_t i = TDATAN::MIN_ADDRESS; i <= TDATAN::MAX_ADDRESS;
         i += TDATAN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<TDATAN>(this);

    for (size_t i = 0; i < csrs.size(); i++)
        if (!csrs[i])
            csrs[i] = std::make_shared<UnimplementedCSR>(this, i, false);

    priv = PrivilegeLevel::M;
}

void Hart::handle_exception(const Trap& trap) noexcept {
    if (trap.cause == Exception::None) [[unlikely]]
        std::terminate();

    // TODO: S mode and U mode

    csrs[MEPC::ADDRESS]->write_unchecked(trap.pc);
    csrs[MCAUSE::ADDRESS]->write_unchecked(static_cast<uint64_t>(trap.cause));
    csrs[MTVAL::ADDRESS]->write_unchecked(trap.tval);

    reg_t mstatus = csrs[MSTATUS::ADDRESS]->read_unchecked();

    if (mstatus & MSTATUS::field::MIE)
        mstatus |= MSTATUS::field::MPIE;
    else
        mstatus &= ~MSTATUS::field::MPIE;

    mstatus &= ~MSTATUS::field::MPP;
    mstatus |= (static_cast<reg_t>(this->priv) << MSTATUS::shift::MPP_SHIFT);
    mstatus &= ~MSTATUS::field::MIE;

    csrs[MSTATUS::ADDRESS]->write_unchecked(mstatus);

    this->pc = csrs[MTVEC::ADDRESS]->read_unchecked() & MTVEC::field::BASE;
    this->priv = PrivilegeLevel::M;
}

} // namespace uemu::core

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

#include <cassert>
#include <exception>
#include <print>

#include "core/decoder.hpp"
#include "core/hart.hpp"
#include "core/mmu.hpp" // IWYU pragma: keep

namespace uemu::core {

reg_t CSR::read_checked(const DecodedInsn& insn) const {
    if (!check_permissions()) [[unlikely]]
        Trap::raise_exception(insn.pc, TrapCause::IllegalInstruction,
                              insn.insn);

    return read_unchecked();
}

void CSR::write_checked(const DecodedInsn& insn, reg_t v) {
    if (!check_permissions()) [[unlikely]]
        Trap::raise_exception(insn.pc, TrapCause::IllegalInstruction,
                              insn.insn);

    write_unchecked(v);
}

[[noreturn]] reg_t
UnimplementedCSR::read_checked(const DecodedInsn& insn) const {
    if (trace_)
        std::println(stderr, "Unimplemented CSR: {:#010x}", address_);

    Trap::raise_exception(insn.pc, TrapCause::IllegalInstruction, insn.insn);
}

[[noreturn]] void UnimplementedCSR::write_checked(const DecodedInsn& insn,
                                                  [[maybe_unused]] reg_t v) {
    if (trace_)
        std::println(stderr, "Unimplemented CSR: {:#010x}", address_);

    Trap::raise_exception(insn.pc, TrapCause::IllegalInstruction, insn.insn);
}

[[noreturn]]
void ConstCSR::write_checked(const DecodedInsn& insn,
                             [[maybe_unused]] reg_t v) {
    Trap::raise_exception(insn.pc, TrapCause::IllegalInstruction, insn.insn);
}

MSTATUS::MSTATUS(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
    using F = MSTATUS::Field;
    using S = MSTATUS::Shift;

    read_mask_ = F::SIE | F::MIE | F::SPIE | F::MPIE | F::SPP | F::MPP | F::FS |
                 F::MPRV | F::SUM | F::MXR | F::TVM | F::TW | F::TSR | F::UXL |
                 F::SXL | F::SD;

    write_mask_ = F::MIE | F::MPIE | F::MPRV | F::MPP | F::FS | F::SIE |
                  F::SPIE | F::SPP | F::SUM | F::MXR | F::TVM | F::TW | F::TSR;

    value_ = (MISA::xlen_64 << S::SXL_SHIFT) | (MISA::xlen_64 << S::UXL_SHIFT) |
             static_cast<reg_t>(PrivilegeLevel::U) << S::MPP_SHIFT;
}

// Implementations are not required to support all MODE settings, and if
// satp is written with an unsupported MODE, the entire write has no effect;
// no fields in satp are modified.
void SATP::write_unchecked(reg_t v) noexcept {
    reg_t mode = (v & MODE) >> MODE_SHIFT;

    if (value_ != v && (mode == Bare || mode == Sv39)) {
        value_ = v;
        hart_->mmu->tlb_flush_all();
    }
}

Hart::Hart(addr_t reset_pc) : pc(reset_pc) {
    // Machine Level
    add_csr<MISA>(MISA::Field::I | MISA::Field::M | MISA::Field::A |
                  MISA::Field::F | MISA::Field::D | MISA::Field::C |
                  MISA::Field::S | MISA::Field::U |
                  (MISA::Mxl::xlen_64 << MISA::Shift::MXL_SHIFT));
    add_csr<MVENDORID>(0);
    add_csr<MARCHID>(0);
    add_csr<MIMPID>(0x00000010);
    add_csr<MHARTID>(0);

    add_csr<MENVCFG>();

    add_csr<MSTATUS>();
    add_csr<MTVEC>();
    add_csr<MEDELEG>();
    add_csr<MIDELEG>();
    add_csr<MIP>();
    add_csr<MIE>();
    add_csr<MSCRATCH>();
    add_csr<MEPC>();
    add_csr<MCAUSE>();
    add_csr<MTVAL>();

    add_csr<MCOUNTEREN>();
    add_csr<MCOUNTINHIBIT>();
    add_csr<MCYCLE>();
    add_csr<MINSTRET>();
    add_csr_ranged<MHPMCOUNTERN>();
    add_csr_ranged<MHPMEVENTN>();

    add_csr<MCONFIGPTR>();

    add_csr_ranged<PMPCFGN>();
    add_csr_ranged<PMPADDRN>();

    add_csr<TSELECT>();
    add_csr_ranged<TDATAN>();

    // Supervisor Level
    add_csr<SSTATUS>();
    add_csr<STVEC>();
    add_csr<SIP>();
    add_csr<SIE>();
    add_csr<SSCRATCH>();
    add_csr<SEPC>();
    add_csr<SCAUSE>();
    add_csr<STVAL>();

    add_csr<SCOUNTEREN>();

    add_csr<SENVCFG>();

    add_csr<SATP>();

    add_csr<STIMECMP>();

    // User Level
    add_csr<CYCLE>();
    add_csr<TIME>();
    add_csr<INSTRET>();

    add_csr<FFLAGS>();
    add_csr<FRM>();
    add_csr<FCSR>();

    for (size_t i = HPMCOUNTERN::MIN_ADDRESS; i <= HPMCOUNTERN::MAX_ADDRESS;
         i += HPMCOUNTERN::DELTA_ADDRESS)
        csrs[i] = std::make_shared<HPMCOUNTERN>(this, i);

    // Unimplemented CSR
    for (size_t i = 0; i < csrs.size(); i++)
        if (!csrs[i])
            csrs[i] = std::make_shared<UnimplementedCSR>(this, i, false);

    // Start with Machine Mode
    priv = PrivilegeLevel::M;
}

void Hart::handle_trap(const Trap& trap) noexcept {
    if (trap.cause == TrapCause::None) [[unlikely]]
        std::terminate();

    const reg_t cause_val = static_cast<reg_t>(trap.cause);
    const bool is_interrupt = (cause_val >> 63) & 1;
    const reg_t cause_code = cause_val & ~(1ULL << 63);

    PrivilegeLevel target_priv = PrivilegeLevel::M;

    if (priv <= PrivilegeLevel::S) {
        reg_t deleg_mask = 0;
        if (is_interrupt)
            deleg_mask = csrs[MIDELEG::ADDRESS]->read_unchecked();
        else
            deleg_mask = csrs[MEDELEG::ADDRESS]->read_unchecked();

        if ((deleg_mask >> cause_code) & 1)
            target_priv = PrivilegeLevel::S;
    }

    if (target_priv == PrivilegeLevel::S) {
        csrs[SEPC::ADDRESS]->write_unchecked(trap.pc);
        csrs[SCAUSE::ADDRESS]->write_unchecked(cause_val);
        csrs[STVAL::ADDRESS]->write_unchecked(trap.tval);

        reg_t sstatus = csrs[SSTATUS::ADDRESS]->read_unchecked();

        if (sstatus & SSTATUS::Field::SIE)
            sstatus |= SSTATUS::Field::SPIE;
        else
            sstatus &= ~SSTATUS::Field::SPIE;

        if (priv >= PrivilegeLevel::S)
            sstatus |= SSTATUS::Field::SPP;
        else
            sstatus &= ~SSTATUS::Field::SPP;

        sstatus &= ~SSTATUS::Field::SIE;

        csrs[SSTATUS::ADDRESS]->write_unchecked(sstatus);

        reg_t stvec = csrs[STVEC::ADDRESS]->read_unchecked();
        reg_t vector_base = stvec & ~3ULL;
        reg_t vector_mode = stvec & 3ULL; // 0: Direct, 1: Vectored

        if (is_interrupt && vector_mode == 1)
            this->pc = vector_base + (cause_code << 2);
        else
            this->pc = vector_base;

        this->priv = PrivilegeLevel::S;
    } else {
        csrs[MEPC::ADDRESS]->write_unchecked(trap.pc);
        csrs[MCAUSE::ADDRESS]->write_unchecked(cause_val);
        csrs[MTVAL::ADDRESS]->write_unchecked(trap.tval);

        reg_t mstatus = csrs[MSTATUS::ADDRESS]->read_unchecked();

        if (mstatus & MSTATUS::Field::MIE)
            mstatus |= MSTATUS::Field::MPIE;
        else
            mstatus &= ~MSTATUS::Field::MPIE;

        mstatus &= ~MSTATUS::Field::MPP;
        mstatus |= (static_cast<reg_t>(priv) << MSTATUS::Shift::MPP_SHIFT);

        mstatus &= ~MSTATUS::MIE;

        csrs[MSTATUS::ADDRESS]->write_unchecked(mstatus);

        reg_t mtvec = csrs[MTVEC::ADDRESS]->read_unchecked();
        reg_t vector_base = mtvec & ~3ULL;
        reg_t vector_mode = mtvec & 3ULL;

        if (is_interrupt && vector_mode == 1)
            this->pc = vector_base + (cause_code << 2);
        else
            this->pc = vector_base;

        this->priv = PrivilegeLevel::M;
    }
}

void Hart::check_interrupts() const {
    const reg_t mip = csrs[MIP::ADDRESS]->read_unchecked();
    const reg_t mie = csrs[MIE::ADDRESS]->read_unchecked();
    const reg_t mstatus = csrs[MSTATUS::ADDRESS]->read_unchecked();
    const reg_t mideleg = csrs[MIDELEG::ADDRESS]->read_unchecked();

    const reg_t pending = mip & mie;

    if (!pending)
        return;

    const reg_t m_pending = pending & ~mideleg;
    const reg_t s_pending = pending & mideleg;

    const bool m_enabled =
        (priv < PrivilegeLevel::M) ||
        (priv == PrivilegeLevel::M && (mstatus & MSTATUS::Field::MIE));
    const bool s_enabled =
        (priv < PrivilegeLevel::S) ||
        (priv == PrivilegeLevel::S && (mstatus & MSTATUS::Field::SIE));

    TrapCause selected_cause = TrapCause::None;

    if (m_enabled && m_pending) {
        if (m_pending & MIP::Field::MEIP)
            selected_cause = TrapCause::MachineExternalInterrupt;
        else if (m_pending & MIP::Field::MSIP)
            selected_cause = TrapCause::MachineSoftwareInterrupt;
        else if (m_pending & MIP::Field::MTIP)
            selected_cause = TrapCause::MachineTimerInterrupt;
    }

    if (selected_cause == TrapCause::None && s_enabled && s_pending) {
        if (s_pending & MIP::Field::SEIP)
            selected_cause = TrapCause::SupervisorExternalInterrupt;
        else if (s_pending & MIP::Field::SSIP)
            selected_cause = TrapCause::SupervisorSoftwareInterrupt;
        else if (s_pending & MIP::Field::STIP)
            selected_cause = TrapCause::SupervisorTimerInterrupt;
    }

    if (selected_cause != TrapCause::None)
        throw Trap(pc, selected_cause, 0);
}

void Hart::set_interrupt_pending(reg_t mip_mask, bool pending) noexcept {
    MIP* mip = dynamic_cast<MIP*>(csrs[MIP::ADDRESS].get());
    assert(mip);

    if (pending)
        mip->set_pending(mip_mask);
    else
        mip->clear_pending(mip_mask);
}

} // namespace uemu::core

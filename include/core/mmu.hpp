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

#pragma once

#include <utility>

#include "core/bus.hpp"
#include "core/decoder.hpp"
#include "core/hart.hpp"

namespace uemu::core {

class MMU {
public:
    enum class AccessType { Fetch, Load, Store };

    explicit MMU(std::shared_ptr<Hart> hart, std::shared_ptr<Bus> bus)
        : reservation_address(0), reservation_valid(false),
          hart_(std::move(hart)), bus_(std::move(bus)) {}

    template <typename T>
    [[nodiscard]] T read(addr_t pc, addr_t addr) {
        constexpr size_t size = sizeof(T);

        // Aligned access
        if (addr % size == 0) [[likely]] {
            addr_t paddr = translate(pc, addr, AccessType::Load);
            std::optional<T> v = bus_->read<T>(paddr);

            if (!v.has_value()) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::LoadAccessFault, addr);

            return *v;
        }

        addr_t paddrs[size];

        for (size_t i = 0; i < size; i++) {
            addr_t vaddr = addr + i;
            paddrs[i] = translate(pc, vaddr, AccessType::Load);

            if (!bus_->accessible(paddrs[i])) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::LoadAccessFault, addr);
        }

        uint8_t bytes[size];

        for (size_t i = 0; i < size; i++) {
            std::optional<uint8_t> v = bus_->read<uint8_t>(paddrs[i]);

            if (!v.has_value()) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::LoadAccessFault, addr);

            bytes[i] = *v;
        }

        T result;
        std::memcpy(&result, bytes, size);
        return result;
    }

    // Write a value of type T to `addr` during instruction execution.
    // Only valid in the instruction execution stage; may throw Trap.
    template <typename T>
    void write(addr_t pc, addr_t addr, T value) {
        constexpr size_t size = sizeof(T);

        // Aligned access
        if (addr % size == 0) [[likely]] {
            addr_t paddr = translate(pc, addr, AccessType::Store);
            bool res = bus_->write<T>(paddr, value);

            if (!res) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);

            return;
        }

        // Unaligned access
        addr_t paddrs[size];

        for (size_t i = 0; i < size; i++) {
            addr_t vaddr = addr + i;
            paddrs[i] = translate(pc, vaddr, AccessType::Store);

            if (!bus_->accessible(paddrs[i])) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);
        }

        // Write data
        uint8_t bytes[size];
        std::memcpy(bytes, &value, size);

        for (size_t i = 0; i < size; i++) {
            bool res = bus_->write<uint8_t>(paddrs[i], bytes[i]);

            if (!res) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);
        }
    }

    // Fetch an instruction from the current PC.
    // Only valid in the instruction fetch stage; may throw Trap.
    [[nodiscard]] std::pair<uint32_t, Ilen> ifetch() {
        const addr_t pc = hart_->pc;

        if (!may_cross_page(pc)) [[likely]] {
            addr_t paddr = translate(pc, pc, AccessType::Fetch);
            std::optional<uint32_t> v = bus_->read<uint32_t>(paddr);

            if (!v.has_value()) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::InstructionAccessFault,
                                      pc);

            uint32_t insn = *v;

            if (Decoder::is_compressed(insn))
                return {insn & 0xFFFF, Ilen::Compressed};

            return {insn, Ilen::Normal};
        }

        // For cross-page instructions, fetch once or twice
        addr_t paddr = translate(pc, pc, AccessType::Fetch);
        std::optional<uint16_t> v = bus_->read<uint16_t>(paddr);

        if (!v.has_value()) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAccessFault, pc);

        uint32_t insn = *v;

        if (Decoder::is_compressed(insn))
            return {insn, Ilen::Compressed};

        paddr = translate(pc, pc + 2, AccessType::Fetch);
        v = bus_->read<uint16_t>(paddr);

        if (!v.has_value()) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::InstructionAccessFault,
                                  pc + 2);

        insn = (insn & 0xFFFF) | (*v << 16);
        return {insn, Ilen::Normal};
    }

    addr_t reservation_address;
    bool reservation_valid;

private:
    static constexpr reg_t PTE_V = 1 << 0;
    static constexpr reg_t PTE_R = 1 << 1;
    static constexpr reg_t PTE_W = 1 << 2;
    static constexpr reg_t PTE_X = 1 << 3;
    static constexpr reg_t PTE_U = 1 << 4;
    static constexpr reg_t PTE_G = 1 << 5;
    static constexpr reg_t PTE_A = 1 << 6;
    static constexpr reg_t PTE_D = 1 << 7;
    static constexpr reg_t PTE_RESERVED_MASK = 0xFFC0000000000000ULL;

    static constexpr size_t LEVELS = 3;
    static constexpr size_t PTESIZE = 8;
    static constexpr size_t PGSHIFT = 12;
    static constexpr size_t VPNBITS = 9;
    static constexpr addr_t PGSIZE = 1ULL << PGSHIFT;
    static constexpr addr_t PGMASK = PGSIZE - 1;

    std::shared_ptr<Hart> hart_;
    std::shared_ptr<Bus> bus_;

    // Check if an instruction at pc may cross pages
    static bool may_cross_page(addr_t pc) noexcept {
        return (pc & (PGSIZE - 1)) == PGSIZE - 2;
    }

    [[noreturn]] void raise_page_fault(addr_t pc, addr_t vaddr,
                                       AccessType type) {
        switch (type) {
            case AccessType::Fetch:
                Trap::raise_exception(pc, TrapCause::InstructionPageFault,
                                      vaddr);
            case AccessType::Load:
                Trap::raise_exception(pc, TrapCause::LoadPageFault, vaddr);
            case AccessType::Store:
                Trap::raise_exception(pc, TrapCause::StoreAMOPageFault, vaddr);
        }

        std::unreachable();
    }

    [[noreturn]] void raise_access_fault(addr_t pc, addr_t vaddr,
                                         AccessType type) {
        switch (type) {
            case AccessType::Fetch:
                Trap::raise_exception(pc, TrapCause::InstructionAccessFault,
                                      vaddr);
            case AccessType::Load:
                Trap::raise_exception(pc, TrapCause::LoadAccessFault, vaddr);
            case AccessType::Store:
                Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault,
                                      vaddr);
        }

        std::unreachable();
    }

    addr_t translate(addr_t pc, addr_t vaddr, AccessType type) {
        PrivilegeLevel priv = hart_->priv;
        reg_t mstatus = hart_->csrs[MSTATUS::ADDRESS]->read_unchecked();

        if (type != AccessType::Fetch && (mstatus & MSTATUS::Field::MPRV)) {
            reg_t mpp =
                (mstatus & MSTATUS::Field::MPP) >> MSTATUS::Shift::MPP_SHIFT;
            priv = static_cast<PrivilegeLevel>(mpp);
        }

        if (priv == PrivilegeLevel::M)
            return vaddr;

        reg_t satp = hart_->csrs[SATP::ADDRESS]->read_unchecked();
        reg_t mode = (satp & SATP::Field::MODE) >> SATP::Shift::MODE_SHIFT;

        if (mode == SATP::Mode::Bare)
            return vaddr;

        if (mode != SATP::Mode::Sv39) [[unlikely]]
            std::terminate();

        int64_t t = static_cast<int64_t>(vaddr << (64 - 39)) >> (64 - 39);
        if (static_cast<addr_t>(t) != vaddr) [[unlikely]]
            raise_page_fault(pc, vaddr, type);

        reg_t ppn = (satp & SATP::Field::PPN) >> SATP::Shift::PPN_SHIFT;
        bool sum = mstatus & MSTATUS::Field::SUM;
        bool mxr = mstatus & MSTATUS::Field::MXR;
        bool adue = hart_->csrs[MENVCFG::ADDRESS]->read_unchecked() &
                    MENVCFG::Field::ADUE;

        int i = static_cast<int>(LEVELS - 1);
        addr_t a = ppn << PGSHIFT;

        while (true) {
            reg_t vpn_i =
                (vaddr >> (PGSHIFT + i * VPNBITS)) & ((1 << VPNBITS) - 1);
            addr_t pte_addr = a + vpn_i * PTESIZE;

            std::optional<uint64_t> pte_opt = bus_->read<uint64_t>(pte_addr);
            if (!pte_opt.has_value()) [[unlikely]]
                raise_access_fault(pc, vaddr, type);

            uint64_t pte = *pte_opt;
            uint64_t pte_ppn = (pte >> 10) & ((1ULL << 44) - 1);

            if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W)))
                [[unlikely]]
                raise_page_fault(pc, vaddr, type);

            // Reserved bits, PBMT bits and PTE_N must not be set
            if (pte >> 54) [[unlikely]]
                raise_page_fault(pc, vaddr, type);

            bool is_leaf = (pte & PTE_R) || (pte & PTE_X);

            if (!is_leaf) {
                // Non-leaf PTE: must have D=A=U=0
                if (pte & (PTE_D | PTE_A | PTE_U))
                    raise_page_fault(pc, vaddr, type);

                if (--i < 0) [[unlikely]]
                    raise_page_fault(pc, vaddr, type);

                a = pte_ppn << PGSHIFT;
                continue;
            }

            if (i > 0) {
                uint64_t mask = (1ULL << (i * VPNBITS)) - 1;
                if (pte_ppn & mask) [[unlikely]]
                    raise_page_fault(pc, vaddr, type);
            }

            bool is_user_page = pte & PTE_U;
            bool s_mode = (priv == PrivilegeLevel::S);

            if (is_user_page) {
                if (s_mode && (type == AccessType::Fetch || !sum))
                    raise_page_fault(pc, vaddr, type);
            } else if (priv == PrivilegeLevel::U) {
                raise_page_fault(pc, vaddr, type);
            }

            // R/W/X permission check
            bool allowed = false;
            switch (type) {
                case AccessType::Fetch: allowed = pte & PTE_X; break;
                case AccessType::Load:
                    allowed = (pte & PTE_R) || (mxr && (pte & PTE_X));
                    break;
                case AccessType::Store: allowed = pte & PTE_W; break;
            }

            if (!allowed) [[unlikely]]
                raise_page_fault(pc, vaddr, type);

            // A/D bit handling
            if (!(pte & PTE_A) ||
                ((type == AccessType::Store) && !(pte & PTE_D))) {
                if (!adue)
                    raise_page_fault(pc, vaddr, type);

                reg_t new_pte = pte | PTE_A;
                if (type == AccessType::Store)
                    new_pte |= PTE_D;

                if (!bus_->write<uint64_t>(pte_addr, new_pte)) [[unlikely]]
                    raise_access_fault(pc, vaddr, type);
            }

            // Construct physical address
            if (i > 0) {
                // Superpage
                reg_t vpn_mask = (1ULL << (i * VPNBITS)) - 1;
                reg_t vpn_low = (vaddr >> PGSHIFT) & vpn_mask;
                reg_t ppn_high_mask = ~vpn_mask;
                reg_t pa_ppn = (pte_ppn & ppn_high_mask) | vpn_low;
                return (pa_ppn << PGSHIFT) | (vaddr & PGMASK);
            }

            return (pte_ppn << PGSHIFT) | (vaddr & PGMASK);
        }

        std::unreachable();
    }
};

} // namespace uemu::core

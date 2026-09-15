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
    static constexpr size_t PGSHIFT = 12;
    static constexpr addr_t PGSIZE = 1ULL << PGSHIFT;
    static constexpr addr_t PGMASK = PGSIZE - 1;

    enum class AccessType { Fetch, Load, Store };

    struct TLBEntry {
        addr_t vpn;
        addr_t ppn;
        uint8_t perm;
        bool valid;
        bool dirty;
        // Host base of the translated DRAM page, or nullptr when that page is
        // not plain DRAM (device, unmapped, or only partly inside DRAM).
        uint8_t* host;
    };

    explicit MMU(Hart* hart, std::shared_ptr<Bus> bus)
        : hart_(hart), bus_(std::move(bus)) {}

    // Read a value of type T from `addr` during instruction execution.
    // When `is_amo` is true, translation and access faults use Store/AMO
    // semantics (cause 7/15) instead of Load semantics (cause 5/13), matching
    // Spike's convert_load_traps_to_store_traps for AMO instructions.
    template <typename T>
    [[nodiscard]] T read(addr_t pc, addr_t addr, bool is_amo = false) {
        constexpr size_t size = sizeof(T);
        AccessType atype = is_amo ? AccessType::Store : AccessType::Load;

        // Aligned access
        if (addr % size == 0) [[likely]] {
            uint8_t* page = nullptr;
            addr_t paddr = translate(pc, addr, atype, &page);

            // translate() allowed the access and its page is whole DRAM, so
            // read the bytes here instead of asking the bus for the physical
            // address.
            if (page) [[likely]] {
                T v;
                std::memcpy(&v, page + (paddr & PGMASK), size);
                return v;
            }

            std::optional<T> v = bus_->read<T>(paddr);

            if (!v.has_value()) [[unlikely]]
                raise_access_fault(pc, addr, atype);

            return *v;
        }

        addr_t paddrs[size];

        for (size_t i = 0; i < size; i++) {
            addr_t vaddr = addr + i;
            paddrs[i] = translate(pc, vaddr, atype);

            if (!bus_->accessible(paddrs[i])) [[unlikely]]
                raise_access_fault(pc, addr, atype);
        }

        uint8_t bytes[size];

        for (size_t i = 0; i < size; i++) {
            std::optional<uint8_t> v = bus_->read<uint8_t>(paddrs[i]);

            if (!v.has_value()) [[unlikely]]
                raise_access_fault(pc, addr, atype);

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
            uint8_t* page = nullptr;
            addr_t paddr = translate(pc, addr, AccessType::Store, &page);

            if (page) [[likely]] {
                std::memcpy(page + (paddr & PGMASK), &value, size);
                return;
            }

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

    // Probe a store address for translation and accessibility with Store/AMO
    // semantics, without actually writing data.  The caller must already have
    // checked natural alignment.  Used by SC to validate the address before
    // evaluating the reservation (matching Spike's check_load_reservation).
    template <typename T>
    void probe_store(addr_t pc, addr_t addr) {
        addr_t paddr = translate(pc, addr, AccessType::Store);
        if (!bus_->accessible(paddr)) [[unlikely]]
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);
    }

    // Fetch an instruction from the current PC.
    // Only valid in the instruction fetch stage; may throw Trap.
    [[nodiscard]] std::pair<uint32_t, Ilen> ifetch() {
        const addr_t pc = hart_->pc;

        // Fast path: the page holding PC was already fetched from, for the
        // privilege level we are still in, and this instruction stays inside
        // that page (so the 4-byte host read cannot leave it; this is not an
        // alignment check).  Such an entry only exists because translate()
        // allowed exactly this fetch, and it is dropped whenever the
        // translation context changes, so the bytes it points at are still the
        // ones the guest would fetch.
        if ((pc & PGMASK) <= PGSIZE - 4 && fetch_page_.valid &&
            fetch_page_.vpn == (pc >> PGSHIFT) &&
            fetch_page_.priv == hart_->priv) [[likely]] {
            uint32_t insn;
            std::memcpy(&insn, fetch_page_.host + (pc & PGMASK), sizeof(insn));

            if (Decoder::is_compressed(insn))
                return {insn & 0xFFFF, Ilen::Compressed};

            return {insn, Ilen::Normal};
        }

        if (!may_cross_page(pc)) [[likely]] {
            addr_t paddr = translate(pc, pc, AccessType::Fetch);
            std::optional<uint32_t> v = bus_->read<uint32_t>(paddr);

            if (!v.has_value()) [[unlikely]]
                Trap::raise_exception(pc, TrapCause::InstructionAccessFault,
                                      pc);

            // Remember the page this fetch came from, while it is plain DRAM:
            // consecutive instructions on it can then be read straight from
            // host memory.  A target that is not DRAM (MMIO, unmapped) clears
            // the entry and keeps taking this path.
            const uint8_t* host = bus_->dram_page_base(paddr);

            fetch_page_.valid = false;

            if (host) {
                fetch_page_.host = host;
                fetch_page_.vpn = pc >> PGSHIFT;
                fetch_page_.priv = hart_->priv;
                fetch_page_.valid = true;
            }

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

    void tlb_flush_all() noexcept {
        memset(itlb_, 0, sizeof(itlb_));
        memset(dtlb_, 0, sizeof(dtlb_));
        fetch_page_.valid = false;
    }

    void tlb_flush_vaddr(addr_t vaddr) {
        uint64_t vpn = vaddr >> PGSHIFT;
        uint32_t idx = vpn & (TLB_ENTRIES - 1);

        if (dtlb_[idx].valid && dtlb_[idx].vpn == vpn)
            dtlb_[idx].valid = false;

        if (itlb_[idx].valid && itlb_[idx].vpn == vpn)
            itlb_[idx].valid = false;

        if (fetch_page_.vpn == vpn)
            fetch_page_.valid = false;
    }

    addr_t reservation_address = 0;
    bool reservation_valid = false;

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
    static constexpr reg_t PTE_PERM_MASK = PTE_R | PTE_W | PTE_X | PTE_U;

    static constexpr size_t LEVELS = 3;
    static constexpr size_t PTESIZE = 8;
    static constexpr size_t VPNBITS = 9;

    static constexpr size_t TLB_ENTRIES = 128;

    Hart* hart_;
    std::shared_ptr<Bus> bus_;

    TLBEntry itlb_[TLB_ENTRIES]{};
    TLBEntry dtlb_[TLB_ENTRIES]{};

    // The guest page the last successful instruction fetch came from, so that
    // consecutive instructions on it are read straight out of DRAM.  This
    // memoizes one completed translation; it is not a second permission model.
    // An entry is only created after translate() allowed the fetch, and it is
    // only used while the page, the privilege level and the translation
    // context are unchanged: the privilege is tagged, and every event that can
    // give a page another translation drops the entry through tlb_flush_all()
    // and tlb_flush_vaddr().  Nothing else can change a fetch's decision,
    // which is why no permission bits are stored here: for an instruction
    // fetch, translate() itself decides only from PTE_X and "a user page is
    // fetched only from U-mode", and both are part of the translation this
    // entry remembers.  If a fetch decision ever depends on more state (PMP,
    // for example), that state has to be tagged or flushed here as well.
    struct {
        const uint8_t* host = nullptr; // first byte of the page in host DRAM
        addr_t vpn = 0;                // guest page number
        PrivilegeLevel priv = PrivilegeLevel::M;
        bool valid = false;
    } fetch_page_;

    // Check if an instruction at pc may cross pages
    static bool may_cross_page(addr_t pc) noexcept {
        return (pc & (PGSIZE - 1)) == PGSIZE - 2;
    }

    [[noreturn]] static void raise_page_fault(addr_t pc, addr_t vaddr,
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

    [[noreturn]] static void raise_access_fault(addr_t pc, addr_t vaddr,
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

    // Translate `vaddr` for an access of `type` and return the guest physical
    // address of the byte being accessed.  When `host_page` is not null it
    // receives the host base of the page holding that physical address, or
    // nullptr when that page is not plain DRAM; every successful return sets
    // it, and only after the checks that allow the access have passed.
    addr_t translate(addr_t pc, addr_t vaddr, AccessType type,
                     uint8_t** host_page = nullptr) {
        PrivilegeLevel priv = hart_->priv;
        reg_t mstatus = hart_->csrs[MSTATUS::ADDRESS]->read_unchecked();

        if (type != AccessType::Fetch && (mstatus & MSTATUS::Field::MPRV)) {
            reg_t mpp =
                (mstatus & MSTATUS::Field::MPP) >> MSTATUS::Shift::MPP_SHIFT;
            priv = static_cast<PrivilegeLevel>(mpp);
        }

        if (priv == PrivilegeLevel::M) {
            if (host_page)
                *host_page = bus_->dram_page_base(vaddr);

            return vaddr;
        }

        reg_t satp = hart_->csrs[SATP::ADDRESS]->read_unchecked();
        reg_t mode = (satp & SATP::Field::MODE) >> SATP::Shift::MODE_SHIFT;

        if (mode == SATP::Mode::Bare) {
            if (host_page)
                *host_page = bus_->dram_page_base(vaddr);

            return vaddr;
        }

        if (mode != SATP::Mode::Sv39) [[unlikely]]
            std::terminate();

        int64_t t = static_cast<int64_t>(vaddr << (64 - 39)) >> (64 - 39);

        // NOLINTNEXTLINE(modernize-use-integer-sign-comparison)
        if (static_cast<addr_t>(t) != vaddr) [[unlikely]]
            raise_page_fault(pc, vaddr, type);

        bool sum = mstatus & MSTATUS::Field::SUM;
        bool mxr = mstatus & MSTATUS::Field::MXR;
        uint64_t vpn = vaddr >> PGSHIFT;
        uint32_t idx = vpn & (TLB_ENTRIES - 1);
        TLBEntry* entry = type == AccessType::Fetch ? &itlb_[idx] : &dtlb_[idx];
        bool allowed = false;

        if (entry->valid && entry->vpn == vpn) [[likely]] {
            if (entry->perm & PTE_U) {
                if (priv == PrivilegeLevel::S &&
                    (type == AccessType::Fetch || !sum))
                    goto miss;
            } else if (priv == PrivilegeLevel::U) {
                goto miss;
            }

            switch (type) {
                case AccessType::Fetch: allowed = entry->perm & PTE_X; break;
                case AccessType::Load:
                    allowed =
                        (entry->perm & PTE_R) || (mxr && (entry->perm & PTE_X));
                    break;
                case AccessType::Store: allowed = entry->perm & PTE_W; break;
            }

            if (!allowed) [[unlikely]]
                goto miss;

            if (type == AccessType::Store && !entry->dirty)
                goto miss;

            if (host_page)
                *host_page = entry->host;

            return (entry->ppn << PGSHIFT) | (vaddr & PGMASK);
        }

    miss:;
        reg_t ppn = (satp & SATP::Field::PPN) >> SATP::Shift::PPN_SHIFT;
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

            // Construct physical address and fill TLB
            uint64_t final_ppn; // NOLINT(cppcoreguidelines-init-variables)

            if (i > 0) {
                // Superpage
                reg_t vpn_mask = (1ULL << (i * VPNBITS)) - 1;
                reg_t vpn_low = (vaddr >> PGSHIFT) & vpn_mask;
                reg_t ppn_high_mask = ~vpn_mask;
                final_ppn = (pte_ppn & ppn_high_mask) | vpn_low;
            } else {
                final_ppn = pte_ppn;
            }

            entry->vpn = vpn;
            entry->ppn = final_ppn;
            entry->perm = pte & PTE_PERM_MASK;
            entry->valid = true;
            entry->dirty = pte & PTE_D;
            entry->host = bus_->dram_page_base(final_ppn << PGSHIFT);

            if (host_page)
                *host_page = entry->host;

            return (final_ppn << PGSHIFT) | (vaddr & PGMASK);
        }

        std::unreachable();
    }
};

} // namespace uemu::core

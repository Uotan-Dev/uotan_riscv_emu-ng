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

#include <gtest/gtest.h>

#include "core/bus.hpp"
#include "core/hart.hpp"
#include "core/mmu.hpp"

namespace uemu::test {

// A device that answers instruction fetches with a fixed instruction and counts
// how often the bus asked it, so a fetch that was cached can be told apart from
// one that went out to the device.
class MockCodeDevice final : public uemu::device::Device {
public:
    MockCodeDevice(addr_t start, size_t size)
        : Device("MockCode", start, size) {}

    [[nodiscard]] size_t reads() const noexcept { return reads_; }

protected:
    std::optional<uint64_t> read_internal(addr_t, size_t) override {
        reads_++;
        return INSN;
    }

    bool write_internal(addr_t, size_t, uint64_t) override { return false; }

private:
    static constexpr uint64_t INSN = 0x00000013; // nop, a full 32-bit insn
    size_t reads_ = 0;
};

// A device that answers data accesses from one register and counts reads and
// writes, so an access served from a cached host page can be told apart from
// one that went out to the device.
class MockDataDevice final : public uemu::device::Device {
public:
    MockDataDevice(addr_t start, size_t size)
        : Device("MockData", start, size) {}

    [[nodiscard]] size_t reads() const noexcept { return reads_; }

    [[nodiscard]] size_t writes() const noexcept { return writes_; }

protected:
    std::optional<uint64_t> read_internal(addr_t, size_t) override {
        reads_++;
        return value_;
    }

    bool write_internal(addr_t, size_t, uint64_t value) override {
        writes_++;
        value_ = value;
        return true;
    }

private:
    size_t reads_ = 0;
    size_t writes_ = 0;
    uint64_t value_ = 0;
};

class MmuTest : public testing::Test {
protected:
    static constexpr addr_t DRAM_BASE = core::Dram::DRAM_BASE;
    static constexpr addr_t PGSIZE = core::Dram::PGSIZE;
    static constexpr size_t DRAM_SIZE = 16 * 1024 * 1024;

    // Page table entries.
    static constexpr uint64_t PTE_V = 1 << 0;
    static constexpr uint64_t PTE_R = 1 << 1;
    static constexpr uint64_t PTE_W = 1 << 2;
    static constexpr uint64_t PTE_X = 1 << 3;
    static constexpr uint64_t PTE_U = 1 << 4;
    static constexpr uint64_t PTE_A = 1 << 6;
    static constexpr uint64_t PTE_D = 1 << 7;
    static constexpr uint64_t PTE_RWX =
        PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D;

    // The address the tests fetch from, and two physical pages to point it at.
    static constexpr addr_t CODE_VA = DRAM_BASE + 0x1000;
    static constexpr addr_t CODE_PA = DRAM_BASE + 0x200000;
    static constexpr addr_t OTHER_PA = DRAM_BASE + 0x201000;

    static constexpr uint32_t INSN_A = 0x00000013;      // nop
    static constexpr uint32_t INSN_B = 0x00100093;      // addi x1, x0, 1
    static constexpr uint32_t INSN_COMPRESSED = 0x0001; // c.nop

    MmuTest()
        : hart(), dram(std::make_shared<core::Dram>(DRAM_SIZE)),
          bus(std::make_shared<core::Bus>(dram)), mmu(&hart, bus) {
        hart.connect_mmu(&mmu);
    }

    [[nodiscard]] uint32_t fetch() { return mmu.ifetch().first; }

    [[nodiscard]] core::Ilen fetch_ilen() { return mmu.ifetch().second; }

    // Install an Sv39 mapping of one 4 KiB page and return the root table.
    addr_t map(addr_t vaddr, addr_t paddr, uint64_t flags) {
        const addr_t root = alloc_table();
        const addr_t mid = alloc_table();
        const addr_t leaf = alloc_table();

        dram->write<uint64_t>(root + ((vaddr >> 30) & 0x1FF) * 8,
                              pte(mid, PTE_V));
        dram->write<uint64_t>(mid + ((vaddr >> 21) & 0x1FF) * 8,
                              pte(leaf, PTE_V));
        dram->write<uint64_t>(leaf + ((vaddr >> 12) & 0x1FF) * 8,
                              pte(paddr, flags));
        return root;
    }

    // Point an existing mapping at another physical page.
    void remap(addr_t root, addr_t vaddr, addr_t paddr, uint64_t flags) {
        const addr_t leaf = leaf_table(root, vaddr);
        dram->write<uint64_t>(leaf + ((vaddr >> 12) & 0x1FF) * 8,
                              pte(paddr, flags));
    }

    void enable_sv39(addr_t root) {
        hart.priv = core::PrivilegeLevel::S;
        hart.csrs[core::SATP::ADDRESS]->write_unchecked(
            (core::SATP::Mode::Sv39 << core::SATP::Shift::MODE_SHIFT) |
            (root >> core::MMU::PGSHIFT));
    }

    void set_sum(bool on) {
        core::CSR* mstatus = hart.csrs[core::MSTATUS::ADDRESS].get();
        reg_t v = mstatus->read_unchecked();

        if (on)
            v |= core::MSTATUS::Field::SUM;
        else
            v &= ~core::MSTATUS::Field::SUM;

        mstatus->write_unchecked(v);
    }

    core::Hart hart;
    std::shared_ptr<core::Dram> dram;
    std::shared_ptr<core::Bus> bus;
    core::MMU mmu;

private:
    [[nodiscard]] static uint64_t pte(addr_t paddr, uint64_t flags) {
        return ((paddr >> core::MMU::PGSHIFT) << 10) | flags;
    }

    addr_t alloc_table() {
        const addr_t addr = table_free_;
        table_free_ += PGSIZE;
        return addr;
    }

    [[nodiscard]] addr_t leaf_table(addr_t root, addr_t vaddr) const {
        const addr_t mid = table_entry(root, (vaddr >> 30) & 0x1FF);
        return table_entry(mid, (vaddr >> 21) & 0x1FF);
    }

    [[nodiscard]] addr_t table_entry(addr_t table, size_t index) const {
        return ((dram->read<uint64_t>(table + index * 8) >> 10) << 12);
    }

    addr_t table_free_ = DRAM_BASE + 0x100000;
};

// Instructions next to each other on one page come back correctly, which is the
// case the current-page cache exists for.
TEST_F(MmuTest, MachineModeFetchesInstructionsFromDram) {
    dram->write<uint32_t>(CODE_VA, INSN_A);
    dram->write<uint32_t>(CODE_VA + 4, INSN_COMPRESSED);

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);
    EXPECT_EQ(fetch_ilen(), core::Ilen::Normal);

    hart.pc += 4;
    EXPECT_EQ(fetch(), INSN_COMPRESSED);
    EXPECT_EQ(fetch_ilen(), core::Ilen::Compressed);
}

// A translated page is used again as long as nothing invalidates it.
TEST_F(MmuTest, Sv39PageIsFetchedThroughTheMapping) {
    dram->write<uint32_t>(CODE_PA, INSN_A);
    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX));

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);
    EXPECT_EQ(fetch(), INSN_A);
}

// SFENCE.VMA (all pages) must retranslate the page, even though the guest page
// number and the privilege level did not change.
TEST_F(MmuTest, TlbFlushAllRetranslatesTheCachedPage) {
    dram->write<uint32_t>(CODE_PA, INSN_A);
    dram->write<uint32_t>(OTHER_PA, INSN_B);

    const addr_t root = map(CODE_VA, CODE_PA, PTE_RWX);
    enable_sv39(root);

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);

    remap(root, CODE_VA, OTHER_PA, PTE_RWX);
    mmu.tlb_flush_all();

    EXPECT_EQ(fetch(), INSN_B);
}

// SFENCE.VMA for this address must do the same.
TEST_F(MmuTest, TlbFlushVaddrRetranslatesTheCachedPage) {
    dram->write<uint32_t>(CODE_PA, INSN_A);
    dram->write<uint32_t>(OTHER_PA, INSN_B);

    const addr_t root = map(CODE_VA, CODE_PA, PTE_RWX);
    enable_sv39(root);

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);

    remap(root, CODE_VA, OTHER_PA, PTE_RWX);
    mmu.tlb_flush_vaddr(CODE_VA);

    EXPECT_EQ(fetch(), INSN_B);
}

// A satp write makes the page translate elsewhere, so the page fetched in bare
// mode must not be served afterwards.
TEST_F(MmuTest, SatpWriteRetranslatesTheCachedPage) {
    dram->write<uint32_t>(CODE_VA, INSN_A);
    dram->write<uint32_t>(OTHER_PA, INSN_B);

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A); // bare, the identity page

    enable_sv39(map(CODE_VA, OTHER_PA, PTE_RWX));

    EXPECT_EQ(fetch(), INSN_B);
}

// The cache is per privilege level: an M-mode fetch is the identity mapping and
// says nothing about what the same page means in S-mode.
TEST_F(MmuTest, PrivilegeChangeRetranslatesTheCachedPage) {
    dram->write<uint32_t>(CODE_VA, INSN_A);
    dram->write<uint32_t>(OTHER_PA, INSN_B);

    enable_sv39(map(CODE_VA, OTHER_PA, PTE_RWX));
    hart.priv = core::PrivilegeLevel::M;

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A); // M-mode ignores satp

    hart.priv = core::PrivilegeLevel::S;

    EXPECT_EQ(fetch(), INSN_B);
}

// A page translated for supervisor use must not be executed after dropping to
// user mode: the fetch has to take the full path and fault.
TEST_F(MmuTest, UserModeDoesNotReuseASupervisorPage) {
    dram->write<uint32_t>(CODE_PA, INSN_A);
    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX));

    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);

    hart.priv = core::PrivilegeLevel::U;

    try {
        static_cast<void>(fetch());
        FAIL() << "user-mode fetch of a supervisor page did not fault";
    } catch (const core::Trap& trap) {
        EXPECT_EQ(trap.cause, core::TrapCause::InstructionPageFault);
    }
}

// The other direction: a user page is executable in U-mode and not in S-mode.
TEST_F(MmuTest, SupervisorDoesNotReuseAUserPage) {
    dram->write<uint32_t>(CODE_PA, INSN_A);
    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX | PTE_U));

    hart.priv = core::PrivilegeLevel::U;
    hart.pc = CODE_VA;
    EXPECT_EQ(fetch(), INSN_A);

    hart.priv = core::PrivilegeLevel::S;

    try {
        static_cast<void>(fetch());
        FAIL() << "supervisor fetch of a user page did not fault";
    } catch (const core::Trap& trap) {
        EXPECT_EQ(trap.cause, core::TrapCause::InstructionPageFault);
    }
}

// A 32-bit instruction at the end of a page is assembled from both pages, and
// a 16-bit instruction there is not extended with the next page's bytes.
TEST_F(MmuTest, CrossPageInstructionIsAssembledFromBothPages) {
    dram->write<uint16_t>(DRAM_BASE + 0xFFE, INSN_A & 0xFFFF);
    dram->write<uint16_t>(DRAM_BASE + 0x1000, INSN_A >> 16);

    hart.pc = DRAM_BASE + 0xFFE;
    EXPECT_EQ(fetch(), INSN_A);
    EXPECT_EQ(fetch_ilen(), core::Ilen::Normal);

    dram->write<uint16_t>(DRAM_BASE + 0xFFE, INSN_COMPRESSED);
    dram->write<uint16_t>(DRAM_BASE + 0x1000, 0xFFFF);

    EXPECT_EQ(fetch(), INSN_COMPRESSED);
    EXPECT_EQ(fetch_ilen(), core::Ilen::Compressed);
}

// Pages that are not plain DRAM (here: a device) keep going through the bus.
TEST_F(MmuTest, DevicePageIsNotCached) {
    constexpr addr_t DEVICE_BASE = 0x1000000;
    auto dev = std::make_shared<MockCodeDevice>(DEVICE_BASE, 0x1000);
    bus->add_device(dev);

    hart.pc = DEVICE_BASE;
    EXPECT_EQ(fetch(), 0x00000013);
    EXPECT_EQ(fetch(), 0x00000013);
    EXPECT_EQ(dev->reads(), 2u);
}

// A mapped DRAM page is reached directly, for loads and stores alike, at the
// offset the access asked for.
TEST_F(MmuTest, Sv39LoadsAndStoresReachTheMappedDramPage) {
    dram->write<uint32_t>(CODE_PA + 0x24, 0xAABBCCDD);
    dram->write<uint64_t>(CODE_PA + 0x1C8, 0x1122334455667788ULL);

    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX));

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x24), 0xAABBCCDDu);
    EXPECT_EQ(mmu.read<uint64_t>(hart.pc, CODE_VA + 0x1C8),
              0x1122334455667788ULL);
    EXPECT_EQ(mmu.read<uint8_t>(hart.pc, CODE_VA + 0x24), 0xDDu);

    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x20, 0x55667788u);
    mmu.write<uint16_t>(hart.pc, CODE_VA + 0x30, 0xBEEFu);

    EXPECT_EQ(dram->read<uint32_t>(CODE_PA + 0x20), 0x55667788u);
    EXPECT_EQ(dram->read<uint16_t>(CODE_PA + 0x30), 0xBEEFu);
}

// Remapping the page and flushing it must drop the cached host page, so later
// accesses use the physical page the mapping names now.
TEST_F(MmuTest, TlbFlushVaddrRefreshesTheCachedHostPage) {
    dram->write<uint32_t>(CODE_PA + 0x40, 0x11111111);
    dram->write<uint32_t>(OTHER_PA + 0x40, 0x22222222);

    const addr_t root = map(CODE_VA, CODE_PA, PTE_RWX);
    enable_sv39(root);

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x40), 0x11111111u);
    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x44, 0x44444444u);

    remap(root, CODE_VA, OTHER_PA, PTE_RWX);
    mmu.tlb_flush_vaddr(CODE_VA);

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x40), 0x22222222u);

    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x44, 0x33333333u);
    EXPECT_EQ(dram->read<uint32_t>(OTHER_PA + 0x44), 0x33333333u);
    EXPECT_EQ(dram->read<uint32_t>(CODE_PA + 0x44), 0x44444444u);
}

// A satp write flushes everything, including the last data access.
TEST_F(MmuTest, SatpWriteDropsTheLastPageCache) {
    dram->write<uint32_t>(CODE_PA + 0x58, 0x11111111);
    dram->write<uint32_t>(OTHER_PA + 0x58, 0x22222222);

    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX));
    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x58), 0x11111111u);
    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x5C, 0x44444444u);

    enable_sv39(map(CODE_VA, OTHER_PA, PTE_RWX));

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x58), 0x22222222u);

    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x5C, 0x33333333u);
    EXPECT_EQ(dram->read<uint32_t>(OTHER_PA + 0x5C), 0x33333333u);
    EXPECT_EQ(dram->read<uint32_t>(CODE_PA + 0x5C), 0x44444444u);
}

// The privilege level is part of what allowed the last access, so an M-mode
// access must not reuse the page translated for S-mode.
TEST_F(MmuTest, PrivilegeChangeDoesNotReuseTheLastPage) {
    dram->write<uint32_t>(CODE_VA + 0x64, 0xAAAAAAAA);
    dram->write<uint32_t>(CODE_PA + 0x64, 0xBBBBBBBB);

    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX));
    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x64), 0xBBBBBBBBu);

    // No flush and no mstatus write here: M-mode ignores satp.
    hart.priv = core::PrivilegeLevel::M;
    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x64), 0xAAAAAAAAu);
}

// A page that belongs to a device is not DRAM, so accesses must keep going
// through the bus and reach the device even though the translation is cached.
TEST_F(MmuTest, DevicePageIsNotServedFromTheHostCache) {
    constexpr addr_t DEVICE_BASE = 0x1000000;
    auto dev = std::make_shared<MockDataDevice>(DEVICE_BASE, PGSIZE);
    bus->add_device(dev);

    enable_sv39(map(CODE_VA, DEVICE_BASE + 0x30, PTE_RWX));

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA), 0u);
    mmu.write<uint32_t>(hart.pc, CODE_VA, 0x55667788u);
    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA), 0x55667788u);

    EXPECT_EQ(dev->reads(), 2u);
    EXPECT_EQ(dev->writes(), 1u);
}

// The cached page is only an address: permission is decided again on every
// access, so clearing SUM takes a mapped DRAM page away again.
TEST_F(MmuTest, UserPageAccessIsRecheckedAgainstSum) {
    dram->write<uint32_t>(CODE_PA + 0x50, 0x0BADF00D);

    enable_sv39(map(CODE_VA, CODE_PA, PTE_RWX | PTE_U));

    try {
        static_cast<void>(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x50));
        FAIL() << "supervisor load of a user page succeeded with SUM clear";
    } catch (const core::Trap& trap) {
        EXPECT_EQ(trap.cause, core::TrapCause::LoadPageFault);
    }

    set_sum(true);
    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x50), 0x0BADF00Du);
    mmu.write<uint32_t>(hart.pc, CODE_VA + 0x54, 0xCAFEBABEu);
    EXPECT_EQ(dram->read<uint32_t>(CODE_PA + 0x54), 0xCAFEBABEu);

    set_sum(false);
    try {
        static_cast<void>(mmu.read<uint32_t>(hart.pc, CODE_VA + 0x50));
        FAIL() << "supervisor load of a user page succeeded after SUM was "
                  "cleared again";
    } catch (const core::Trap& trap) {
        EXPECT_EQ(trap.cause, core::TrapCause::LoadPageFault);
    }

    try {
        mmu.write<uint32_t>(hart.pc, CODE_VA + 0x58, 0xDEADBEEFu);
        FAIL() << "supervisor store to a user page succeeded after SUM was "
                  "cleared again";
    } catch (const core::Trap& trap) {
        EXPECT_EQ(trap.cause, core::TrapCause::StoreAMOPageFault);
    }

    EXPECT_EQ(dram->read<uint32_t>(CODE_PA + 0x58), 0u);
}

// In M-mode the address is the physical address, and the page is whole DRAM,
// so the access needs no bus lookup either.
TEST_F(MmuTest, MachineModeLoadsAndStoresReachDram) {
    dram->write<uint32_t>(DRAM_BASE + 0x60, 0xDEADBEEF);
    dram->write<uint64_t>(DRAM_BASE + 0x68, 0x1122334455667788ULL);

    EXPECT_EQ(mmu.read<uint32_t>(hart.pc, DRAM_BASE + 0x60), 0xDEADBEEFu);
    EXPECT_EQ(mmu.read<uint64_t>(hart.pc, DRAM_BASE + 0x68),
              0x1122334455667788ULL);

    mmu.write<uint16_t>(hart.pc, DRAM_BASE + 0x70, 0xBEEF);
    EXPECT_EQ(dram->read<uint16_t>(DRAM_BASE + 0x70), 0xBEEFu);
}

} // namespace uemu::test

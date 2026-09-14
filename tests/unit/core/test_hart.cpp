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

#include <random>

#include <gtest/gtest.h>

#include "core/bus.hpp"
#include "core/decoder.hpp"
#include "core/hart.hpp"
#include "core/mmu.hpp"

namespace uemu::test {

TEST(RegisterFileTest, X0IsHardwiredToZero) {
    core::RegisterFile regs;

    // Writing a non-zero value to x0 should have no effect
    regs.write(0, 0xDEADBEEF);
    EXPECT_EQ(regs.read(0), 0);
    EXPECT_EQ(regs[0], 0);
}

TEST(RegisterFileTest, ReadWriteGeneralPurposeRegisters) {
    std::mt19937 rng(0x12345678);
    std::uniform_int_distribution<reg_t> dist(
        1, std::numeric_limits<reg_t>::max());
    core::RegisterFile regs;

    for (size_t i = 1; i < core::Hart::GPR_COUNT; i++) {
        reg_t v = dist(rng);
        regs.write(i, v);
        EXPECT_EQ(regs.read(i), v);
        EXPECT_EQ(regs[i], v);
    }
}

class MipTest : public testing::Test {
protected:
    static constexpr uint8_t RD = 5;
    static constexpr uint8_t RS1 = 6;

    MipTest()
        : hart(), dram(std::make_shared<core::Dram>(4096)),
          bus(std::make_shared<core::Bus>(dram)), mmu(&hart, bus),
          mip(dynamic_cast<core::MIP*>(hart.csrs[core::MIP::ADDRESS].get())) {}

    void execute_csr(uint32_t funct3, reg_t source) {
        hart.gprs.write(RS1, source);
        const uint32_t insn = (core::MIP::ADDRESS << 20) | (RS1 << 15) |
                              (funct3 << 12) | (RD << 7) | 0x73;
        const core::DecodedInsn decoded = core::Decoder::decode(
            insn, core::Ilen::Normal, core::Dram::DRAM_BASE);
        decoded(hart, mmu);
    }

    core::Hart hart;
    std::shared_ptr<core::Dram> dram;
    std::shared_ptr<core::Bus> bus;
    core::MMU mmu;
    core::MIP* mip;
};

TEST_F(MipTest, ExternalSeipSurvivesCsrWrite) {
    ASSERT_NE(mip, nullptr);
    mip->set_pending(core::MIP::SEIP);

    execute_csr(0b001, 0); // CSRRW

    EXPECT_EQ(hart.gprs[RD], core::MIP::SEIP);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::SEIP, core::MIP::SEIP);
    mip->clear_pending(core::MIP::SEIP);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::SEIP, 0);
}

TEST_F(MipTest, CsrrsDoesNotLatchExternalSeip) {
    ASSERT_NE(mip, nullptr);
    mip->set_pending(core::MIP::SEIP);

    execute_csr(0b010, 0); // CSRRS with a nonzero rs1 field

    EXPECT_EQ(hart.gprs[RD], core::MIP::SEIP);
    mip->clear_pending(core::MIP::SEIP);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::SEIP, 0);
}

TEST_F(MipTest, CsrrcClearsOnlySoftwareSeip) {
    ASSERT_NE(mip, nullptr);
    mip->write_unchecked(core::MIP::SEIP);
    mip->set_pending(core::MIP::SEIP);

    execute_csr(0b011, core::MIP::SEIP); // CSRRC

    EXPECT_EQ(hart.gprs[RD], core::MIP::SEIP);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::SEIP, core::MIP::SEIP);
    mip->clear_pending(core::MIP::SEIP);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::SEIP, 0);
}

// Interrupt selection must consider M-mode destinations before S-mode ones:
// a delegated supervisor interrupt may not preempt an interrupt that is
// destined for M-mode.
class InterruptPriorityTest : public testing::Test {
protected:
    InterruptPriorityTest() {
        hart.priv = core::PrivilegeLevel::S;
        hart.csrs[core::MSTATUS::ADDRESS]->write_unchecked(
            core::MSTATUS::Field::SIE);
    }

    void enable_in_mie(reg_t bits) {
        hart.csrs[core::MIE::ADDRESS]->write_unchecked(bits);
    }

    void delegate(reg_t bits) {
        hart.csrs[core::MIDELEG::ADDRESS]->write_unchecked(bits);
    }

    [[nodiscard]] core::TrapCause take_interrupt() const {
        try {
            hart.check_interrupts();
        } catch (const core::Trap& trap) { return trap.cause; }

        return core::TrapCause::None;
    }

    core::Hart hart;
};

TEST_F(InterruptPriorityTest, MachineTargetedInterruptWinsOverDelegated) {
    delegate(core::MIDELEG::Field::SSIP);
    enable_in_mie(core::MIE::Field::SSIE | core::MIE::Field::STIE);
    hart.set_interrupt_pending(core::MIP::Field::SSIP | core::MIP::Field::STIP,
                               true);

    EXPECT_EQ(take_interrupt(), core::TrapCause::SupervisorTimerInterrupt);
}

TEST_F(InterruptPriorityTest, DelegatedInterruptWinsWhenNoMachinePending) {
    delegate(core::MIDELEG::Field::SSIP);
    enable_in_mie(core::MIE::Field::SSIE | core::MIE::Field::STIE);
    hart.set_interrupt_pending(core::MIP::Field::SSIP, true);

    EXPECT_EQ(take_interrupt(), core::TrapCause::SupervisorSoftwareInterrupt);
}

TEST_F(InterruptPriorityTest, MachineModeWithoutMieDefersInterrupts) {
    hart.priv = core::PrivilegeLevel::M;
    hart.csrs[core::MSTATUS::ADDRESS]->write_unchecked(0);
    delegate(0);
    enable_in_mie(core::MIE::Field::STIE);
    hart.set_interrupt_pending(core::MIP::Field::STIP, true);

    EXPECT_EQ(take_interrupt(), core::TrapCause::None);
}

} // namespace uemu::test

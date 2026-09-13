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

#include <limits>

#include "core/hart.hpp"
#include "device/clint.hpp"

namespace uemu::test {

TEST(ClintTest, MTIMECMPTrigger) {
    constexpr size_t MTIMECMP_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MTIMECMP_OFFSET;

    auto hart = std::make_shared<uemu::core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(hart, 1000);

    std::ignore = clint.write(MTIMECMP_ADDR, 1145141919810ull);
    clint.tick();
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MTIP);

    bool r = clint.write<uint64_t>(MTIMECMP_ADDR, 0ull);
    std::this_thread::sleep_for(std::chrono::milliseconds(64));
    clint.tick();
    EXPECT_TRUE(r);
    EXPECT_TRUE(mip->read_unchecked() & core::MIP::MTIP);

    r = clint.write<uint64_t>(MTIMECMP_ADDR, 1145141919810ull);
    EXPECT_TRUE(r);
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MTIP);
}

TEST(ClintTest, MSIPWrite) {
    constexpr size_t MSIP_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MSIP_OFFSET;

    auto hart = std::make_shared<uemu::core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(hart, 1000);

    // Write 1 to MSIP
    bool r = clint.write<uint32_t>(MSIP_ADDR, 1);
    EXPECT_TRUE(r);
    EXPECT_TRUE(mip->read_unchecked() & core::MIP::MSIP);

    // Write 0 to MSIP
    r = clint.write<uint32_t>(MSIP_ADDR, 0);
    EXPECT_TRUE(r);
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MSIP);
}

TEST(ClintTest, DeterministicTimer) {
    constexpr size_t MTIME_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MTIME_OFFSET;
    constexpr size_t MTIMECMP_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MTIMECMP_OFFSET;

    auto hart = std::make_shared<core::Hart>();
    auto* mip = dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    auto* menvcfg =
        dynamic_cast<core::MENVCFG*>(hart->csrs[core::MENVCFG::ADDRESS].get());
    auto* stimecmp = dynamic_cast<core::STIMECMP*>(
        hart->csrs[core::STIMECMP::ADDRESS].get());
    auto* time =
        dynamic_cast<core::TIME*>(hart->csrs[core::TIME::ADDRESS].get());
    ASSERT_NE(mip, nullptr);
    ASSERT_NE(menvcfg, nullptr);
    ASSERT_NE(stimecmp, nullptr);
    ASSERT_NE(time, nullptr);

    device::Clint clint(hart, device::Clint::DEFAULT_FREQ,
                        TimerMode::Deterministic);
    ASSERT_TRUE(clint.write<uint64_t>(MTIMECMP_ADDR,
                                      std::numeric_limits<uint64_t>::max()));

    clint.advance_timer(3);
    EXPECT_EQ(clint.get_mtime(), 3);
    EXPECT_EQ(time->read_unchecked(), 3);

    ASSERT_TRUE(clint.write<uint64_t>(MTIME_ADDR, 42));
    clint.advance_timer(1);
    EXPECT_EQ(clint.get_mtime(), 43);

    ASSERT_TRUE(clint.write<uint64_t>(MTIMECMP_ADDR, 43));
    EXPECT_NE(mip->read_unchecked() & core::MIP::Field::MTIP, 0);
    ASSERT_TRUE(clint.write<uint64_t>(MTIMECMP_ADDR,
                                      std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(mip->read_unchecked() & core::MIP::Field::MTIP, 0);

    menvcfg->write_unchecked(core::MENVCFG::Field::STCE);
    stimecmp->write_unchecked(44);
    EXPECT_EQ(mip->read_unchecked() & core::MIP::Field::STIP, 0);
    clint.advance_timer(1);
    EXPECT_NE(mip->read_unchecked() & core::MIP::Field::STIP, 0);
}

} // namespace uemu::test

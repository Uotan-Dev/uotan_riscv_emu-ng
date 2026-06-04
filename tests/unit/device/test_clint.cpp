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

#include "core/hart.hpp"
#include "device/clint.hpp"

namespace uemu::test {

TEST(ClintTest, MtimeAdvance) {
    constexpr auto N = device::Clint::INSNS_PER_RTC_TICK;

    auto hart = std::make_shared<core::Hart>();
    device::Clint clint(hart);

    // mtime starts at 0
    EXPECT_EQ(clint.get_mtime(), 0);

    // N-1 updates: mtime should stay at 0
    for (size_t i = 0; i < N - 1; i++)
        clint.update();
    EXPECT_EQ(clint.get_mtime(), 0);

    // Nth update: cross boundary, mtime becomes 1
    clint.update();
    EXPECT_EQ(clint.get_mtime(), 1);

    // Another N updates: mtime becomes 2
    for (size_t i = 0; i < N; i++)
        clint.update();
    EXPECT_EQ(clint.get_mtime(), 2);
}

TEST(ClintTest, MTIMECMPTrigger) {
    constexpr auto N = device::Clint::INSNS_PER_RTC_TICK;
    constexpr addr_t MTIMECMP_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MTIMECMP_OFFSET;

    auto hart = std::make_shared<core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(hart);

    // Set mtimecmp = 5 → triggers when mtime reaches 5
    bool r = clint.write<uint64_t>(MTIMECMP_ADDR, 5);
    EXPECT_TRUE(r);

    // Advance to mtime = 4: 4 * N = 400 updates.
    // After this, cycle_count_ == 400, at a boundary (400 % 100 == 0).
    for (size_t i = 0; i < 4 * N; i++)
        clint.update();
    EXPECT_EQ(clint.get_mtime(), 4);
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MTIP);

    // Advance to mtime = 5: need N more updates to reach cycle_count_ = 500.
    for (size_t i = 0; i < N; i++)
        clint.update();
    EXPECT_EQ(clint.get_mtime(), 5);
    EXPECT_TRUE(mip->read_unchecked() & core::MIP::MTIP);

    // Re-arm: set mtimecmp larger than current mtime → MTIP cleared
    r = clint.write<uint64_t>(MTIMECMP_ADDR, 10);
    EXPECT_TRUE(r);
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MTIP);
}

TEST(ClintTest, MSIPWrite) {
    constexpr addr_t MSIP_ADDR =
        device::Clint::DEFAULT_BASE + device::Clint::MSIP_OFFSET;

    auto hart = std::make_shared<core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(hart);

    // Write 1 to MSIP
    bool r = clint.write<uint32_t>(MSIP_ADDR, 1);
    EXPECT_TRUE(r);
    EXPECT_TRUE(mip->read_unchecked() & core::MIP::MSIP);

    // Write 0 to MSIP
    r = clint.write<uint32_t>(MSIP_ADDR, 0);
    EXPECT_TRUE(r);
    EXPECT_FALSE(mip->read_unchecked() & core::MIP::MSIP);
}

} // namespace uemu::test

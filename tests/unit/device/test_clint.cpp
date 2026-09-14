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

TEST(ClintTest, MTIMECMPTrigger) {
    // A 1 kHz timebase keeps the test short.
    ClintConfig config;
    config.freq_hz = 1000;

    auto hart = std::make_shared<uemu::core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(config, hart);

    const addr_t MTIMECMP_ADDR = clint.start() + device::Clint::MTIMECMP_OFFSET;

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
    ClintConfig config;
    config.freq_hz = 1000;

    auto hart = std::make_shared<uemu::core::Hart>();
    core::MIP* mip =
        dynamic_cast<core::MIP*>(hart->csrs[core::MIP::ADDRESS].get());
    ASSERT_NE(mip, nullptr);

    device::Clint clint(config, hart);

    const addr_t MSIP_ADDR = clint.start() + device::Clint::MSIP_OFFSET;

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

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

#include "core/hart.hpp"

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
    std::uniform_int_distribution<reg_t> dist(1, std::numeric_limits<reg_t>::max());
    core::RegisterFile regs;

    for (size_t i = 1; i < core::Hart::GPR_COUNT; i++) {
        reg_t v = dist(rng);
        regs.write(i, v);
        EXPECT_EQ(regs.read(i), v);
        EXPECT_EQ(regs[i], v);
    }
}

} // namespace uemu::test

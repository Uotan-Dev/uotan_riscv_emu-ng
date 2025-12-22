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

#include <gtest/gtest.h>

#include "common/bit.hpp"

namespace uemu::test {

TEST(BitTest, Bitmask) {
    EXPECT_EQ(bitmask<uint64_t>(0), 0x0ULL);
    EXPECT_EQ(bitmask<uint32_t>(-1), 0x0U);
    EXPECT_EQ(bitmask<uint32_t>(8), 0xFFU);
    EXPECT_EQ(bitmask<uint64_t>(12), 0xFFFULL);
    EXPECT_EQ(bitmask<uint8_t>(8), 0xFFU);
    EXPECT_EQ(bitmask<uint64_t>(64), std::numeric_limits<uint64_t>::max());
    EXPECT_EQ(bitmask<uint16_t>(32), 0xFFFFU);
}

TEST(BitTest, BitsExtraction) {
    uint64_t val = 0xABCD'1234'5678'90EFULL;
    int32_t sval = -1;

    EXPECT_EQ(bits(val, 7, 0), 0xEFULL);
    EXPECT_EQ(bits(val, 15, 12), 0x9ULL);
    EXPECT_EQ(bits(val, 63, 63), 1ULL);
    EXPECT_EQ(bits(sval, 7, 0), 0xFFU);
}

TEST(BitTest, SignExtension) {
    EXPECT_EQ(sext(0x7FFULL, 12), 2047LL);
    EXPECT_EQ(sext(0x800ULL, 12), -2048LL);
    EXPECT_EQ(sext(0xFFFF'FFFFULL, 32), -1LL);
    EXPECT_EQ(sext(0x7FFF'FFFFULL, 32), 2147483647LL);
    EXPECT_EQ(sext(0x8000'0000'0000'0000ULL, 64),
              static_cast<int64_t>(0x8000'0000'0000'0000ULL));
}

} // namespace uemu::test

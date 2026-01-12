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

#include "device/simple_fb.hpp"

namespace uemu::test {

class SimpleFBTest : public ::testing::Test {
protected:
    void SetUp() override {
        update_count = 0;
        last_update_data = nullptr;

        fb = std::make_shared<device::SimpleFB>();
    }

    void TearDown() override { fb.reset(); }

    std::shared_ptr<device::SimpleFB> fb;
    std::atomic<int> update_count;
    const uint8_t* last_update_data;
};

TEST_F(SimpleFBTest, SingleByteAccess) {
    addr_t base = fb->start();

    EXPECT_TRUE(fb->write<uint8_t>(base, 0xFF));

    auto val = fb->read<uint8_t>(base);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0xFF);
}

TEST_F(SimpleFBTest, MultiByteAccess) {
    addr_t base = fb->start();

    // Write 32-bit pixel (RGBA)
    uint32_t pixel = 0xAABBCCDD;
    EXPECT_TRUE(fb->write<uint32_t>(base, pixel));

    // Read back
    auto val = fb->read<uint32_t>(base);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, pixel);
}

TEST_F(SimpleFBTest, 64BitAccess) {
    addr_t base = fb->start();

    // Write 64 bits (two pixels)
    uint64_t value = 0x1122334455667788ULL;
    EXPECT_TRUE(fb->write<uint64_t>(base, value));

    // Read back
    auto val = fb->read<uint64_t>(base);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, value);
}

TEST_F(SimpleFBTest, OutOfBoundsAccess) {
    addr_t base = fb->start();
    size_t size = fb->size();

    // Try to read beyond framebuffer
    auto val = fb->read<uint32_t>(base + size);
    EXPECT_FALSE(val.has_value());

    // Try to write beyond framebuffer
    EXPECT_FALSE(fb->write<uint32_t>(base + size, 0x12345678));

    // Partial out of bounds (4 bytes starting at size-2)
    val = fb->read<uint32_t>(base + size - 2);
    EXPECT_FALSE(val.has_value());

    EXPECT_FALSE(fb->write<uint32_t>(base + size - 2, 0x12345678));
}

TEST_F(SimpleFBTest, UnalignedAccess) {
    addr_t base = fb->start();

    // Write at unaligned address (offset 1)
    EXPECT_TRUE(fb->write<uint32_t>(base + 1, 0xAABBCCDD));

    // Read back
    auto val = fb->read<uint32_t>(base + 1);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0xAABBCCDD);

    // Verify byte layout (little-endian)
    auto b0 = fb->read<uint8_t>(base + 1);
    auto b1 = fb->read<uint8_t>(base + 2);
    auto b2 = fb->read<uint8_t>(base + 3);
    auto b3 = fb->read<uint8_t>(base + 4);

    ASSERT_TRUE(b0.has_value() && b1.has_value() && b2.has_value() &&
                b3.has_value());
    EXPECT_EQ(*b0, 0xDD);
    EXPECT_EQ(*b1, 0xCC);
    EXPECT_EQ(*b2, 0xBB);
    EXPECT_EQ(*b3, 0xAA);
}

} // namespace uemu::test

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

#include "core/dram.hpp"

namespace uemu::test {

class DramTest : public ::testing::Test {
protected:
    void SetUp() override {
        dram = std::make_unique<core::Dram>(TEST_DRAM_SIZE);
    }

    // 128MB for testing
    static constexpr size_t TEST_DRAM_SIZE = 128 * 1024 * 1024;
    std::unique_ptr<core::Dram> dram;
};

// Check if address validation correctly handles DRAM_BASE and boundaries.
TEST_F(DramTest, AddressValidation) {
    // Valid start address
    EXPECT_TRUE(dram->is_valid_addr(core::Dram::DRAM_BASE));

    // Valid end address
    EXPECT_TRUE(
        dram->is_valid_addr(core::Dram::DRAM_BASE + TEST_DRAM_SIZE - 1));

    // Invalid addresses below DRAM_BASE
    EXPECT_FALSE(dram->is_valid_addr(0x0));
    EXPECT_FALSE(dram->is_valid_addr(core::Dram::DRAM_BASE - 1));
    EXPECT_FALSE(dram->is_valid_addr(0x3f));

    // Invalid addresses beyond size
    EXPECT_FALSE(dram->is_valid_addr(core::Dram::DRAM_BASE + TEST_DRAM_SIZE));
    EXPECT_FALSE(
        dram->is_valid_addr(core::Dram::DRAM_BASE + TEST_DRAM_SIZE * 2));

    // Multi-byte length validation
    EXPECT_TRUE(dram->is_valid_addr(core::Dram::DRAM_BASE, 8));
    EXPECT_FALSE(
        dram->is_valid_addr(core::Dram::DRAM_BASE + TEST_DRAM_SIZE - 4, 8));
}

// Test template-based read and write for various integer types.
TEST_F(DramTest, TemplateReadWrite) {
    uemu::addr_t target_addr = core::Dram::DRAM_BASE + 0x100;

    uint64_t val64 = 0xDEADBEEFCAFEBABE;
    dram->write<uint64_t>(target_addr, val64);
    EXPECT_EQ(dram->read<uint64_t>(target_addr), val64);

    uint32_t val32 = 0x12345678;
    dram->write<uint32_t>(target_addr + 8, val32);
    EXPECT_EQ(dram->read<uint32_t>(target_addr + 8), val32);

    uint8_t val8 = 0xFF;
    dram->write<uint8_t>(target_addr + 12, val8);
    EXPECT_EQ(dram->read<uint8_t>(target_addr + 12), val8);
}

// Test bulk data operations
TEST_F(DramTest, BulkByteOperations) {
    const char* secret = "RISC-V is awesome!";
    size_t len = std::strlen(secret) + 1;
    uemu::addr_t addr = core::Dram::DRAM_BASE + 0x200;
    char buffer[64]{};

    dram->write_bytes(addr, secret, len);
    dram->read_bytes(addr, buffer, len);

    EXPECT_STREQ(secret, buffer);
}

// Test that out-of-bounds byte operations throw std::out_of_range.
TEST_F(DramTest, ExceptionHandling) {
    uint8_t dummy[10]{};

    addr_t oob_addr = core::Dram::DRAM_BASE + TEST_DRAM_SIZE + 0x1000;
    EXPECT_THROW(dram->write_bytes(oob_addr, dummy, 10), std::out_of_range);
    EXPECT_THROW(dram->read_bytes(oob_addr, dummy, 10), std::out_of_range);

    addr_t edge_addr = core::Dram::DRAM_BASE + TEST_DRAM_SIZE - 5;
    EXPECT_THROW(dram->read_bytes(edge_addr, dummy, 10), std::out_of_range);
}

} // namespace uemu::test

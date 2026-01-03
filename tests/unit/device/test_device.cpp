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

#include "device/device.hpp"

namespace uemu::test {

class TestDevice : public device::Device {
public:
    TestDevice() : Device("Test", 0x1000, 0x100) {}

    uint64_t read_internal(addr_t, size_t) override { return 0; }

    void write_internal(addr_t, size_t, uint64_t) override {}

    static void test_read_le(const void* src, addr_t off, size_t sz,
                             uint64_t* out) {
        read_little_endian(src, off, sz, out);
    }

    static void test_write_le(void* dst, addr_t off, size_t sz, uint64_t val) {
        write_little_endian(dst, off, sz, val);
    }
};

TEST(DeviceTest, ReadLittleEndian) {
    uint64_t source = 0x0102030405060708ULL;
    uint64_t val = 0;

    // Read full 64-bit
    TestDevice::test_read_le(&source, 0, 8, &val);
    EXPECT_EQ(val, 0x0102030405060708ULL);

    // Read lower 32-bit (offsets 0-3) -> expect 0x05060708
    val = 0;
    TestDevice::test_read_le(&source, 0, 4, &val);
    EXPECT_EQ(val, 0x05060708ULL);

    // Read upper 32-bit (offsets 4-7) -> expect 0x01020304
    val = 0;
    TestDevice::test_read_le(&source, 4, 4, &val);
    EXPECT_EQ(val, 0x01020304ULL);

    // Read 1 byte at offset 1 -> expect 0x07
    val = 0;
    TestDevice::test_read_le(&source, 1, 1, &val);
    EXPECT_EQ(val, 0x07ULL);
}

TEST(DeviceTest, WriteLittleEndian) {
    uint64_t dest = 0;

    // Write full 64-bit
    TestDevice::test_write_le(&dest, 0, 8, 0xAABBCCDDEEFF1122ULL);
    EXPECT_EQ(dest, 0xAABBCCDDEEFF1122ULL);

    // Write lower 32-bit
    dest = 0;
    TestDevice::test_write_le(&dest, 0, 4, 0xDEADBEEF);
    EXPECT_EQ(dest, 0xDEADBEEFULL);

    // Write upper 32-bit
    dest = 0;
    TestDevice::test_write_le(&dest, 4, 4, 0xCAFEBABE);
    EXPECT_EQ(dest, 0xCAFEBABE00000000ULL);

    // Case 4: Mixed write
    dest = 0xFFFFFFFFFFFFFFFFULL;
    TestDevice::test_write_le(&dest, 2, 2, 0x1234);
    EXPECT_EQ(dest, 0xFFFFFFFF1234FFFFULL);
}

} // namespace uemu::test

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

namespace uemu::test {

// A simple Mock Device for testing Bus routing
class MockDevice : public uemu::device::Device {
public:
    MockDevice(addr_t start, size_t size) : Device("Mock", start, size) {}

    uint64_t read_internal(addr_t, size_t) override { return 0x42; }

    void write_internal(addr_t, size_t, uint64_t val) override {
        last_val = val;
    }

    uint64_t last_val = 0;
};

TEST(BusTest, DramRouting) {
    auto dram = std::make_shared<core::Dram>(1024 * 1024);
    core::Bus bus(dram);

    // Access DRAM base
    addr_t addr = core::Dram::DRAM_BASE;
    bool res = bus.write<uint32_t>(addr, 0x11223344);
    EXPECT_TRUE(res);

    auto val = bus.read<uint32_t>(addr);

    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0x11223344);
}

TEST(BusTest, DeviceRouting) {
    auto dram = std::make_shared<core::Dram>(1024);
    core::Bus bus(dram);
    auto dev = std::make_shared<MockDevice>(0x1000, 0x100);
    bus.add_device(dev);

    // Test reading from device
    auto val = bus.read<uint8_t>(0x1000);
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 0x42);

    // Test writing to device
    bool res = bus.write<uint32_t>(0x1004, 0x99);
    EXPECT_TRUE(res);
    EXPECT_EQ(dev->last_val, 0x99);
}

TEST(BusTest, RejectOverlappingDevices) {
    auto dram = std::make_shared<core::Dram>(0x1000);
    core::Bus bus(dram);

    // Device overlaps with DRAM (DRAM_BASE is usually 0x80000000)
    auto bad_dev =
        std::make_shared<MockDevice>(core::Dram::DRAM_BASE + 0x100, 0x100);
    EXPECT_THROW(bus.add_device(bad_dev), std::runtime_error);
}

} // namespace uemu::test

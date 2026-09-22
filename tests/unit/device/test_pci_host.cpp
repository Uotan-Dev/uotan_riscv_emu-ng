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

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "board_config.hpp"
#include "core/bus.hpp"
#include "core/dram.hpp"
#include "device/pci_host.hpp"

namespace uemu::test {

namespace {

using device::PciFunction;
using device::PciHost;
using device::PciMemoryBar;

constexpr size_t PCI_COMMAND = 0x04;
constexpr size_t PCI_CLASS_REVISION = 0x08;
constexpr size_t PCI_HEADER_TYPE = 0x0e;
constexpr size_t PCI_BASE_ADDRESS_0 = 0x10;
constexpr uint16_t PCI_COMMAND_MEMORY = 0x2;

addr_t config_address(const PciHostConfig& config, uint8_t device,
                      uint8_t function, size_t offset) {
    return config.ecam_base + (static_cast<addr_t>(device) << 15) +
           (static_cast<addr_t>(function) << 12) + offset;
}

class PciHostTest : public ::testing::Test {
protected:
    PciHostTest()
        : dram_(std::make_shared<core::Dram>(1024 * 1024)), bus_(dram_),
          host_(config_) {
        bus_.add_device(host_.ecam_device());
        bus_.add_device(host_.mmio_device());
    }

    PciMemoryBar memory_bar() {
        return PciMemoryBar(
            storage_.size(),
            [this](size_t offset, size_t size) -> std::optional<uint64_t> {
                if (offset > storage_.size() || size > storage_.size() - offset)
                    return std::nullopt;

                uint64_t value = 0;
                for (size_t byte = 0; byte < size; ++byte)
                    value |= static_cast<uint64_t>(storage_[offset + byte])
                             << (byte * 8);
                return value;
            },
            [this](size_t offset, size_t size, uint64_t value) {
                if (offset > storage_.size() || size > storage_.size() - offset)
                    return false;

                for (size_t byte = 0; byte < size; ++byte)
                    storage_[offset + byte] =
                        static_cast<uint8_t>(value >> (byte * 8));
                return true;
            });
    }

    PciHostConfig config_;
    std::shared_ptr<core::Dram> dram_;
    core::Bus bus_;
    PciHost host_;
    std::array<uint8_t, 4096> storage_{};
};

} // namespace

TEST_F(PciHostTest, HostBridgeAndAbsentFunctionsHaveStandardConfigValues) {
    EXPECT_EQ(bus_.read<uint32_t>(config_address(config_, 0, 0, 0)),
              0x00081b36u);
    EXPECT_EQ(
        bus_.read<uint32_t>(config_address(config_, 0, 0, PCI_CLASS_REVISION)),
        0x06000000u);
    EXPECT_EQ(
        bus_.read<uint8_t>(config_address(config_, 0, 0, PCI_HEADER_TYPE)), 0u);

    // A conventional Type-0 function only exposes the first 256 config bytes.
    EXPECT_EQ(bus_.read<uint32_t>(config_address(config_, 0, 0, 0x100)),
              0xffffffffu);
    EXPECT_EQ(bus_.read<uint8_t>(config_address(config_, 1, 0, 0)), 0xffu);
    EXPECT_EQ(bus_.read<uint16_t>(config_address(config_, 1, 0, 0)), 0xffffu);
    EXPECT_EQ(bus_.read<uint32_t>(config_address(config_, 1, 0, 0)),
              0xffffffffu);
}

TEST_F(PciHostTest, MemoryBarRequiresMemoryEnableAndFollowsRelocation) {
    PciFunction function({.vendor_id = 0x1234,
                          .device_id = 0x5678,
                          .revision = 1,
                          .class_code = 0x03});
    function.set_memory_bar(0, memory_bar());
    host_.register_function({.device = 1, .function = 0}, std::move(function));

    const addr_t first = config_.mmio_base;
    const addr_t second = config_.mmio_base + 0x4000;
    const addr_t bar = config_address(config_, 1, 0, PCI_BASE_ADDRESS_0);
    const addr_t command = config_address(config_, 1, 0, PCI_COMMAND);

    ASSERT_TRUE(bus_.write<uint32_t>(bar, first + 0x321));
    EXPECT_EQ(bus_.read<uint32_t>(bar), first);
    EXPECT_EQ(bus_.read<uint32_t>(first + 4), 0xffffffffu);
    EXPECT_TRUE(bus_.write<uint32_t>(first + 4, 0xfeedface));

    ASSERT_TRUE(bus_.write<uint16_t>(command, PCI_COMMAND_MEMORY));
    ASSERT_TRUE(bus_.write<uint32_t>(first + 4, 0xfeedface));
    EXPECT_EQ(bus_.read<uint32_t>(first + 4), 0xfeedfaceu);

    ASSERT_TRUE(bus_.write<uint32_t>(bar, second));
    EXPECT_EQ(bus_.read<uint32_t>(first + 4), 0xffffffffu);
    EXPECT_TRUE(bus_.write<uint32_t>(first + 4, 0));
    ASSERT_TRUE(bus_.write<uint32_t>(second + 4, 0x12345678));
    EXPECT_EQ(bus_.read<uint32_t>(second + 4), 0x12345678u);

    ASSERT_TRUE(bus_.write<uint16_t>(command, 0));
    EXPECT_EQ(bus_.read<uint32_t>(second + 4), 0xffffffffu);
    EXPECT_TRUE(bus_.write<uint32_t>(second + 4, 0));
}

TEST_F(PciHostTest, BarSizingUsesItsNormalWritableAddressMask) {
    PciFunction function({.vendor_id = 0x1234, .device_id = 0x5678});
    function.set_memory_bar(0, memory_bar());
    host_.register_function({.device = 1, .function = 0}, std::move(function));

    const addr_t original = config_.mmio_base + 0x8000;
    const addr_t bar = config_address(config_, 1, 0, PCI_BASE_ADDRESS_0);
    const addr_t command = config_address(config_, 1, 0, PCI_COMMAND);
    ASSERT_TRUE(bus_.write<uint32_t>(bar, original));
    ASSERT_TRUE(bus_.write<uint16_t>(command, PCI_COMMAND_MEMORY));
    ASSERT_TRUE(bus_.write<uint8_t>(original, 0x55));

    // Firmware sizes a BAR by preserving its old value, writing all ones,
    // reading the hardwired address mask, then restoring the saved value.
    ASSERT_TRUE(bus_.write<uint32_t>(bar, 0xffffffffu));
    EXPECT_EQ(bus_.read<uint32_t>(bar), 0xfffff000u);
    EXPECT_EQ(bus_.read<uint8_t>(original), 0xffu);
    EXPECT_TRUE(bus_.write<uint8_t>(original, 0));

    ASSERT_TRUE(bus_.write<uint32_t>(bar, original));
    EXPECT_EQ(bus_.read<uint8_t>(original), 0x55u);
}

TEST_F(PciHostTest, UnimplementedBarsRemainZero) {
    PciFunction function({.vendor_id = 0x1234, .device_id = 0x5678});
    host_.register_function({.device = 1, .function = 0}, std::move(function));

    const addr_t bar = config_address(config_, 1, 0, PCI_BASE_ADDRESS_0);
    EXPECT_EQ(bus_.read<uint32_t>(bar), 0u);
    ASSERT_TRUE(bus_.write<uint32_t>(bar, 0xffffffffu));
    EXPECT_EQ(bus_.read<uint32_t>(bar), 0u);
}

TEST_F(PciHostTest, UnmappedApertureReadsAllOnesAndDropsWrites) {
    const addr_t hole = config_.mmio_base + 0x20000;

    EXPECT_EQ(bus_.read<uint8_t>(hole), 0xffu);
    EXPECT_EQ(bus_.read<uint16_t>(hole), 0xffffu);
    EXPECT_EQ(bus_.read<uint32_t>(hole), 0xffffffffu);
    EXPECT_TRUE(bus_.write<uint8_t>(hole, 0));
    EXPECT_TRUE(bus_.write<uint16_t>(hole, 0));
    EXPECT_TRUE(bus_.write<uint32_t>(hole, 0));
}

TEST_F(PciHostTest, CommandAndBarSupportByteAndWordWrites) {
    PciFunction function({.vendor_id = 0x1234, .device_id = 0x5678});
    function.set_memory_bar(0, memory_bar());
    host_.register_function({.device = 1, .function = 0}, std::move(function));

    const addr_t command = config_address(config_, 1, 0, PCI_COMMAND);
    const addr_t bar = config_address(config_, 1, 0, PCI_BASE_ADDRESS_0);
    const addr_t initial = config_.mmio_base + 0x10000;
    const addr_t relocated = config_.mmio_base + 0x12000;

    // Only the implemented MSE bit is writable, regardless of access width.
    ASSERT_TRUE(bus_.write<uint8_t>(command + 1, 0xff));
    EXPECT_EQ(bus_.read<uint16_t>(command), 0u);
    ASSERT_TRUE(bus_.write<uint8_t>(command, PCI_COMMAND_MEMORY));
    EXPECT_EQ(bus_.read<uint16_t>(command), PCI_COMMAND_MEMORY);

    ASSERT_TRUE(bus_.write<uint16_t>(bar + 2, 0x6001));
    EXPECT_EQ(bus_.read<uint32_t>(bar), initial);
    ASSERT_TRUE(bus_.write<uint8_t>(bar + 1, 0x20));
    EXPECT_EQ(bus_.read<uint32_t>(bar), relocated);
    EXPECT_EQ(bus_.read<uint32_t>(initial), 0xffffffffu);
    EXPECT_TRUE(bus_.write<uint32_t>(relocated, 0x12345678));
    EXPECT_EQ(bus_.read<uint32_t>(relocated), 0x12345678u);

    ASSERT_TRUE(bus_.write<uint8_t>(command, 0));
    EXPECT_EQ(bus_.read<uint16_t>(command), 0u);
    EXPECT_EQ(bus_.read<uint32_t>(relocated), 0xffffffffu);
}

TEST(PciHostConfigTest, RejectsWindowsOutsideTheSupportedEcamAndMmioModel) {
    PciHostConfig config;
    config.ecam_size = PciHost::ECAM_BUS_SIZE * 2;
    EXPECT_THROW(PciHost host(config), std::invalid_argument);

    config = {};
    config.ecam_base += 4;
    EXPECT_THROW(PciHost host(config), std::invalid_argument);

    config = {};
    config.mmio_base = 0xfffff000;
    config.mmio_size = 0x2000;
    EXPECT_THROW(PciHost host(config), std::invalid_argument);
}

} // namespace uemu::test

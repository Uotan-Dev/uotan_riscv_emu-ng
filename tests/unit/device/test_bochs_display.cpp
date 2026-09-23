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

#include <memory>

#include <gtest/gtest.h>

#include "board_config.hpp"
#include "core/bus.hpp"
#include "core/dram.hpp"
#include "device/bochs_display.hpp"
#include "device/pci_host.hpp"

namespace uemu::test {
namespace {

class BochsDisplayTest : public ::testing::Test {
protected:
    BochsDisplayTest()
        : bus_(std::make_shared<core::Dram>(1024 * 1024)), host_(config_),
          display_(std::make_shared<device::BochsDisplay>(DisplayConfig{})) {
        bus_.add_device(host_.ecam_device());
        bus_.add_device(host_.mmio_device());
        host_.register_function({.device = 1, .function = 0},
                                display_->pci_function());
    }

    addr_t pci_config(size_t offset) const {
        return config_.ecam_base + (addr_t{1} << 15) + offset;
    }

    void enable_bars() {
        ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x10), VRAM_BASE));
        ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x18), MMIO_BASE));
        ASSERT_TRUE(bus_.write<uint16_t>(pci_config(0x04), 2));
    }

    addr_t reg(size_t index) const { return MMIO_BASE + 0x500 + index * 2; }

    static constexpr addr_t VRAM_BASE = 0x60000000;
    static constexpr addr_t MMIO_BASE = 0x64000000;

    PciHostConfig config_;
    core::Bus bus_;
    device::PciHost host_;
    std::shared_ptr<device::BochsDisplay> display_;
};

} // namespace

TEST_F(BochsDisplayTest, IdentityAndBarsExposeOnlyImplementedFunctions) {
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(0)), 0x11111234u);
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(8)), 0x03800001u);
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(0x14)), 0u);
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(0x1c)), 0u);

    ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x10), 0xffffffffu));
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(0x10)), 0xfc000000u);
    ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x18), 0xffffffffu));
    EXPECT_EQ(bus_.read<uint32_t>(pci_config(0x18)), 0xfffff000u);

    enable_bars();
    EXPECT_EQ(bus_.read<uint16_t>(reg(0)), 0xb0c5u);
    EXPECT_EQ(bus_.read<uint16_t>(reg(10)), 1024u);
    EXPECT_EQ(bus_.read<uint32_t>(MMIO_BASE + 0x600), 0xffffffffu);
    EXPECT_EQ(bus_.read<uint8_t>(MMIO_BASE + 0x400), 0xffu);
    EXPECT_TRUE(bus_.write<uint8_t>(MMIO_BASE + 0x420, 0x20));
}

TEST_F(BochsDisplayTest, VramFollowsMemoryEnableAndBarRelocation) {
    enable_bars();
    ASSERT_TRUE(bus_.write<uint32_t>(VRAM_BASE + 4, 0x11223344));
    EXPECT_EQ(bus_.read<uint32_t>(VRAM_BASE + 4), 0x11223344u);
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->pixels()[4], 0x44u);
    }

    ASSERT_TRUE(bus_.write<uint16_t>(pci_config(0x04), 0));
    EXPECT_EQ(bus_.read<uint32_t>(VRAM_BASE + 4), 0xffffffffu);
    ASSERT_TRUE(bus_.write<uint16_t>(pci_config(0x04), 2));
    ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x10), 0x68000000u));
    EXPECT_EQ(bus_.read<uint32_t>(VRAM_BASE + 4), 0xffffffffu);
    EXPECT_EQ(bus_.read<uint32_t>(0x68000004u), 0x11223344u);

    ASSERT_TRUE(bus_.write<uint32_t>(pci_config(0x18), MMIO_BASE + 0x1000));
    EXPECT_EQ(bus_.read<uint16_t>(reg(0)), 0xffffu);
    EXPECT_EQ(bus_.read<uint16_t>(MMIO_BASE + 0x1000 + 0x500), 0xb0c5u);
}

TEST_F(BochsDisplayTest, ModesetUpdatesLockedGeometryAndScanoutOffset) {
    enable_bars();
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(1), 640));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(2), 480));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(3), 32));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(6), 800));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0xc1)); // enable, LFB, no-clear
    EXPECT_EQ(bus_.read<uint16_t>(reg(1)), 640u);
    ASSERT_TRUE(bus_.write<uint32_t>(VRAM_BASE + 4, 0xaabbccdd));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(8), 1));
    {
        auto lock = display_->lock();
        const auto geometry = display_->geometry();
        EXPECT_EQ(geometry.width, 640u);
        EXPECT_EQ(geometry.height, 480u);
        EXPECT_EQ(geometry.stride, 800u * 4);
        EXPECT_EQ(display_->pixels()[0], 0xddu);
    }

    ASSERT_TRUE(bus_.write<uint16_t>(reg(9), 1));
    ASSERT_TRUE(bus_.write<uint32_t>(VRAM_BASE + 3204, 0x12345678));
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->pixels()[0], 0x78u);
    }
    ASSERT_TRUE(bus_.write<uint16_t>(reg(9), 0xffff));
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->pixels()[0], 0x78u);
        EXPECT_EQ(display_->geometry().stride, 3200u);
    }
}

TEST_F(BochsDisplayTest, UnsupportedBppCannotChangeScanout) {
    enable_bars();
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(3), 16));
    EXPECT_EQ(bus_.read<uint16_t>(reg(3)), 32u);
    ASSERT_TRUE(bus_.write<uint16_t>(reg(1), 9000));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0xc1));
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->geometry().width, 1024u);
    }
}

TEST_F(BochsDisplayTest, GuestCanChooseAModeLargerThanTheInitialSize) {
    enable_bars();
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(1), 1920));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(2), 1080));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(6), 1920));
    ASSERT_TRUE(bus_.write<uint16_t>(reg(4), 0x41));
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->geometry().width, 1920u);
        EXPECT_EQ(display_->geometry().height, 1080u);
        EXPECT_EQ(display_->geometry().stride, 1920u * 4);
    }

    ASSERT_TRUE(bus_.write<uint8_t>(reg(4), 0));
    ASSERT_TRUE(bus_.write<uint8_t>(reg(1), 0x80));
    ASSERT_TRUE(bus_.write<uint8_t>(reg(1) + 1, 0x02));
    EXPECT_EQ(bus_.read<uint16_t>(reg(1)), 640u);
    ASSERT_TRUE(bus_.write<uint8_t>(reg(4), 0xc1));
    {
        auto lock = display_->lock();
        EXPECT_EQ(display_->geometry().width, 640u);
    }
}

} // namespace uemu::test

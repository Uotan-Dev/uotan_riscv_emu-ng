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
#include <stdexcept>

#include <gtest/gtest.h>

#include "board_config.hpp"
#include "core/dram.hpp"
#include "core/hart.hpp"
#include "device/clint.hpp"
#include "device/ns16550.hpp"
#include "device/pflash_cfi01.hpp"
#include "device/plic.hpp"
#include "device/simple_fb.hpp"

namespace uemu::test {

// The board defaults are the guest-visible machine ABI: README's device table
// and FdtGenerator describe the same map, and firmware depends on it.  Pinning
// every default here keeps a later change from silently moving a device.
TEST(BoardConfigTest, DefaultsMatchTheDocumentedMachineLayout) {
    const BoardConfig board;

    EXPECT_EQ(board.dram.size, 512u * 1024u * 1024u);

    EXPECT_EQ(board.uart.base, 0x10000000u);
    EXPECT_EQ(board.uart.size, 0x100u);
    EXPECT_EQ(board.uart.interrupt_id, 10u);
    EXPECT_EQ(board.uart.clock_hz, 3686400u);
    EXPECT_EQ(board.uart.reg_shift, 0u);
    EXPECT_EQ(board.uart.reg_io_width, 1u);

    EXPECT_EQ(board.clint.base, 0x2000000u);
    EXPECT_EQ(board.clint.size, 0x10000u);
    EXPECT_EQ(board.clint.freq_hz, 10000000u);

    EXPECT_EQ(board.plic.base, 0xc000000u);
    EXPECT_EQ(board.plic.size, 0x1000000u);
    EXPECT_EQ(board.plic.ndev, 31u);

    // One cfi-flash node: two banks that share their geometry.
    EXPECT_EQ(board.flash0.base, 0x20000000u);
    EXPECT_EQ(board.flash1.base, 0x22000000u);
    for (const PFlashConfig* bank : {&board.flash0, &board.flash1}) {
        EXPECT_EQ(bank->sector_len, 0x10000u);
        EXPECT_EQ(bank->num_blocks, 512u);
        EXPECT_TRUE(bank->image.empty());
    }

    EXPECT_EQ(board.framebuffer.base, 0x50000000u);
    EXPECT_EQ(board.framebuffer.width, 1024u);
    EXPECT_EQ(board.framebuffer.height, 768u);

    EXPECT_EQ(board.virtio_blk.base, 0x10001000u);
    EXPECT_EQ(board.virtio_blk.size, 0x1000u);
    EXPECT_EQ(board.virtio_blk.interrupt_id, 12u);
    EXPECT_TRUE(board.virtio_blk.image.empty());

    EXPECT_EQ(board.rtc.base, 0x101000u);
    EXPECT_EQ(board.rtc.size, 0x100u);
    EXPECT_EQ(board.rtc.interrupt_id, 11u);

    EXPECT_EQ(board.input.base, 0x10002000u);
    EXPECT_EQ(board.input.size, 0x1000u);
    EXPECT_EQ(board.input.interrupt_id, 2u);
    EXPECT_EQ(board.input.device_name, "qwerty2");

    EXPECT_EQ(board.battery.base, 0x10003000u);
    EXPECT_EQ(board.battery.size, 0x1000u);
    EXPECT_EQ(board.battery.interrupt_id, 3u);
    EXPECT_EQ(board.battery.capacity, 96u);

    EXPECT_EQ(board.rng.base, 0x10004000u);
    EXPECT_EQ(board.rng.size, 0x10u);

    EXPECT_EQ(board.nemu_console.base, 0x10008000u);
    EXPECT_EQ(board.nemu_console.size, 8u);

    EXPECT_EQ(board.sifive_test.base, 0x100000u);
    EXPECT_EQ(board.sifive_test.size, 0x1000u);

    EXPECT_EQ(board.test_intr_gen.base, 0x40000000u);
    EXPECT_EQ(board.test_intr_gen.size, 0x1000u);
}

TEST(BoardConfigTest, TwoFlashBanksGetDistinctWindows) {
    const BoardConfig board;

    const device::PFlashCFI01 flash0(board.flash0);
    const device::PFlashCFI01 flash1(board.flash1);

    EXPECT_EQ(flash0.start(), 0x20000000u);
    EXPECT_EQ(flash0.size(), 0x2000000u);
    EXPECT_EQ(flash1.start(), 0x22000000u);
    EXPECT_EQ(flash1.size(), 0x2000000u);
}

// A window the device cannot decode would make the bus unable to see the
// region is taken, so the device refuses to be built with one.
TEST(BoardConfigTest, RejectsZeroSizedRegions) {
    PFlashConfig flash;
    flash.num_blocks = 0;
    EXPECT_THROW(device::PFlashCFI01 bank(flash), std::invalid_argument);

    SimpleFBConfig framebuffer;
    framebuffer.width = 0;
    EXPECT_THROW(device::SimpleFB screen(framebuffer), std::invalid_argument);
}

TEST(BoardConfigTest, RejectsZeroSizedDram) {
    EXPECT_THROW(core::Dram dram(0), std::invalid_argument);
}

TEST(BoardConfigTest, RejectsAZeroTimebase) {
    ClintConfig config;
    config.freq_hz = 0;

    auto hart = std::make_shared<core::Hart>();
    EXPECT_THROW(device::Clint clint(config, hart), std::invalid_argument);
}

TEST(BoardConfigTest, RejectsAnOutOfRangePlicSourceCount) {
    PlicConfig config;
    config.ndev = device::Plic::MAX_DEVICES; // one past the last usable id

    auto hart = std::make_shared<core::Hart>();
    EXPECT_THROW(device::Plic plic(config, hart), std::invalid_argument);
}

TEST(BoardConfigTest, RejectsAnImpossibleUartShift) {
    NS16550Config config;
    config.reg_shift = 64; // shifting a 64-bit offset by this much is undefined

    const auto no_irq = [](uint32_t, bool) {};
    device::ConsoleChannel channel;
    EXPECT_THROW(device::NS16550 uart(config, no_irq, channel),
                 std::invalid_argument);
}

} // namespace uemu::test

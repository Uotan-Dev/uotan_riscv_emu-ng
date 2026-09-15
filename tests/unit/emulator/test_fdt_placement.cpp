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
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <libfdt.h>

#include "board_config.hpp"
#include "core/dram.hpp"
#include "device/sifive_test.hpp"
#include "emulator.hpp"
#include "fdt_generator.hpp"

namespace uemu::test {

namespace {

// A board small enough that the highest 2 MiB slot is easy to reason about.
BoardConfig small_board() {
    BoardConfig config;
    config.dram.size = 16 * 1024 * 1024;
    return config;
}

// One MiB of zero bytes, used as a stand-in for a loaded image:
// Emulator::load() records the range, which is all the device tree placement
// looks at.
std::vector<uint8_t> image_bytes() { return std::vector<uint8_t>(1024 * 1024); }

} // namespace

TEST(EmulatorFdtTest, PlacementUsesTheHighestGapBetweenLoadedRanges) {
    const BoardConfig config = small_board();
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();
    constexpr addr_t alignment = 2 * 1024 * 1024;
    const addr_t first_candidate =
        (core::Dram::DRAM_BASE + config.dram.size - dtb.size()) &
        ~(alignment - 1);
    const addr_t lower = core::Dram::DRAM_BASE + 8 * 1024 * 1024;
    const std::vector<uint8_t> image = image_bytes();
    Emulator emulator(config);

    // The highest slot is taken, the next one down is free.
    emulator.load(lower, image);
    emulator.load(first_candidate, image);
    const addr_t address = emulator.install_fdt(dtb);

    ASSERT_EQ(emulator.loaded_ranges().size(), 2u);
    const AddressRange fdt_range{address, address + dtb.size()};
    for (const AddressRange& range : emulator.loaded_ranges())
        EXPECT_FALSE(fdt_range.overlaps(range));
    EXPECT_EQ(address, first_candidate - alignment);

    std::vector<uint8_t> installed(dtb.size());
    emulator.read(address, installed.data(), installed.size());
    EXPECT_EQ(installed, dtb);
    EXPECT_EQ(fdt_check_header(installed.data()), 0);
}

TEST(EmulatorFdtTest, BootRegisterPointsAtTheInstalledTree) {
    // auipc t0, 0; sd a1, 0x100(t0); then report PASS through SiFiveTest.
    constexpr std::array<uint8_t, 28> firmware = {
        0x97, 0x02, 0x00, 0x00, 0x23, 0xb0, 0xb2, 0x10, 0xb7, 0x02,
        0x10, 0x00, 0x37, 0x53, 0x00, 0x00, 0x13, 0x03, 0x53, 0x55,
        0x23, 0xa0, 0x62, 0x00, 0x6f, 0x00, 0x00, 0x00,
    };
    const BoardConfig config = small_board();
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();
    Emulator emulator(config);
    emulator.load(core::Dram::DRAM_BASE, firmware.data(), firmware.size());
    const addr_t address = emulator.install_fdt(dtb);

    emulator.run();

    addr_t boot_a1 = 0;
    emulator.read(core::Dram::DRAM_BASE + 0x100, &boot_a1, sizeof(boot_a1));
    EXPECT_EQ(boot_a1, address);
    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

TEST(EmulatorFdtTest, RejectsADeviceTreeThatDoesNotFit) {
    BoardConfig config;
    config.dram.size = 1024;
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();
    Emulator emulator(config);

    EXPECT_THROW(static_cast<void>(emulator.install_fdt(dtb)),
                 std::runtime_error);
}

TEST(EmulatorFdtTest, RejectsADeviceTreeWhenDramIsFullyOccupied) {
    BoardConfig config;
    config.dram.size = 1024 * 1024;
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();
    Emulator emulator(config);
    emulator.load(core::Dram::DRAM_BASE, image_bytes());

    // The blob would fit in DRAM, but no aligned slot is free: the placement
    // must fail instead of overwriting the loaded image.
    EXPECT_THROW(static_cast<void>(emulator.install_fdt(dtb)),
                 std::runtime_error);
}

} // namespace uemu::test

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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

#include "core/dram.hpp"
#include "device/sifive_test.hpp"
#include "device/simple_fb.hpp"
#include "emulator.hpp"

namespace uemu::test {

namespace {

constexpr size_t TEST_DRAM_SIZE = 16 * 1024 * 1024;

// Every test below runs the default board with a smaller DRAM.
BoardConfig default_board() {
    BoardConfig config;
    config.dram.size = TEST_DRAM_SIZE;
    return config;
}

// j .
constexpr std::array<uint8_t, 4> JIG_FIRMWARE = {0x6f, 0x00, 0x00, 0x00};

// lui  t0, 0x100            ; t0 = 0x100000 (SiFiveTest)
// lui  t1, 0x5
// addi t1, t1, 0x555        ; t1 = 0x5555
// sw   t1, 0(t0)            ; shutdown PASS with code 0
// j    .
constexpr std::array<uint8_t, 20> HALT_FIRMWARE = {
    0xb7, 0x02, 0x10, 0x00, 0x37, 0x53, 0x00, 0x00, 0x13, 0x03,
    0x53, 0x55, 0x23, 0xa0, 0x62, 0x00, 0x6f, 0x00, 0x00, 0x00,
};

// lui  t0, 0x10000          ; t0 = 0x10000000 (NS16550)
// addi t1, zero, 'A'
// sb   t1, 0(t0)            ; one console byte (the UART is byte-wide)
// then the shutdown sequence above
constexpr std::array<uint8_t, 32> UART_THEN_HALT_FIRMWARE = {
    0xb7, 0x02, 0x00, 0x10, 0x13, 0x03, 0x10, 0x04, 0x23, 0x80, 0x62,
    0x00, 0xb7, 0x02, 0x10, 0x00, 0x37, 0x53, 0x00, 0x00, 0x13, 0x03,
    0x53, 0x55, 0x23, 0xa0, 0x62, 0x00, 0x6f, 0x00, 0x00, 0x00,
};

// The same program, with the UART write moved to an address the default board
// leaves free: `lui t0, 0x11000` instead of `lui t0, 0x10000`.
constexpr std::array<uint8_t, 32> RELOCATED_UART_THEN_HALT_FIRMWARE = {
    0xb7, 0x02, 0x00, 0x11, 0x13, 0x03, 0x10, 0x04, 0x23, 0x80, 0x62,
    0x00, 0xb7, 0x02, 0x10, 0x00, 0x37, 0x53, 0x00, 0x00, 0x13, 0x03,
    0x53, 0x55, 0x23, 0xa0, 0x62, 0x00, 0x6f, 0x00, 0x00, 0x00,
};

template <typename T>
void load_firmware(Emulator& emulator, const T& firmware) {
    emulator.load(core::Dram::DRAM_BASE, firmware.data(), firmware.size());
}

} // namespace

TEST(EmulatorTest, StartAndStopWithoutGuestHalt) {
    Emulator emulator(default_board());
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.start();
    EXPECT_FALSE(emulator.finished());

    emulator.request_shutdown();
    emulator.wait();

    EXPECT_TRUE(emulator.finished());
}

TEST(EmulatorTest, GuestHaltReportsStatus) {
    Emulator emulator(default_board());
    load_firmware(emulator, HALT_FIRMWARE);

    emulator.run();

    EXPECT_TRUE(emulator.finished());
    EXPECT_EQ(emulator.shutdown_code(), 0);
    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

TEST(EmulatorTest, RunHonoursTheTimeout) {
    Emulator emulator(default_board());
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.run(std::chrono::milliseconds(50));

    EXPECT_TRUE(emulator.finished());
}

TEST(EmulatorTest, ConsoleOutputReachesTheFrontendPort) {
    Emulator emulator(default_board());
    load_firmware(emulator, UART_THEN_HALT_FIRMWARE);

    emulator.run();

    ASSERT_EQ(emulator.console_count(), 2u);
    EXPECT_EQ(emulator.console_name(0), "NS16550A");
    EXPECT_EQ(emulator.console_name(1), "NEMU Console");
    EXPECT_TRUE(emulator.console_accepts_input(0));
    EXPECT_FALSE(emulator.console_accepts_input(1));
    EXPECT_EQ(emulator.console_output(0), "A");
    EXPECT_TRUE(emulator.console_output(1).empty());
}

TEST(EmulatorTest, StartTwiceThrows) {
    Emulator emulator(default_board());
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.start();
    EXPECT_THROW(emulator.start(), std::logic_error);

    emulator.request_shutdown();
    emulator.wait();
}

TEST(EmulatorTest, WaitWithoutStartIsNoop) {
    Emulator emulator(default_board());

    EXPECT_NO_THROW(emulator.wait());
    EXPECT_FALSE(emulator.finished());
}

TEST(EmulatorTest, DramSizeComesFromTheConfig) {
    Emulator emulator(default_board());

    const std::array<uint8_t, 4> word{};
    const addr_t last_word = core::Dram::DRAM_BASE + TEST_DRAM_SIZE - 4;

    EXPECT_NO_THROW(emulator.load(last_word, word.data(), word.size()));
    EXPECT_THROW(emulator.load(last_word + 4, word.data(), word.size()),
                 std::out_of_range);
}

TEST(EmulatorTest, FramebufferGeometryComesFromTheConfig) {
    BoardConfig config = default_board();
    config.framebuffer.width = 320;
    config.framebuffer.height = 200;

    Emulator emulator(config);

    EXPECT_EQ(emulator.framebuffer().width(), 320u);
    EXPECT_EQ(emulator.framebuffer().height(), 200u);
    EXPECT_EQ(emulator.framebuffer().byte_size(),
              320u * 200u * device::SimpleFB::BPP);
}

TEST(EmulatorTest, UartBaseComesFromTheConfig) {
    BoardConfig config = default_board();
    config.uart.base = 0x11000000; // free: no disk image is configured

    Emulator emulator(config);
    load_firmware(emulator, RELOCATED_UART_THEN_HALT_FIRMWARE);

    // The guest still halts through SiFiveTest, wherever the UART went.
    emulator.run(std::chrono::milliseconds(2000));

    EXPECT_EQ(emulator.console_output(0), "A");
    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

TEST(EmulatorTest, OverlappingDeviceWindowsAreRejected) {
    BoardConfig config = default_board();
    config.uart.base = config.clint.base; // on top of the CLINT window

    EXPECT_THROW(Emulator emulator(config), std::runtime_error);
}

} // namespace uemu::test

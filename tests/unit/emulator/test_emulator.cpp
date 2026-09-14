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
#include "emulator.hpp"

namespace uemu::test {

namespace {

constexpr size_t TEST_DRAM_SIZE = 16 * 1024 * 1024;

// j .
constexpr std::array<uint8_t, 4> JIG_FIRMWARE = {
    0x6f, 0x00, 0x00, 0x00,
};

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
    0xb7, 0x02, 0x00, 0x10, 0x13, 0x03, 0x10, 0x04, 0x23, 0x80,
    0x62, 0x00, 0xb7, 0x02, 0x10, 0x00, 0x37, 0x53, 0x00, 0x00,
    0x13, 0x03, 0x53, 0x55, 0x23, 0xa0, 0x62, 0x00, 0x6f, 0x00,
    0x00, 0x00,
};

template <typename T>
void load_firmware(Emulator& emulator, const T& firmware) {
    emulator.load(core::Dram::DRAM_BASE, firmware.data(), firmware.size());
}

} // namespace

TEST(EmulatorTest, StartAndStopWithoutGuestHalt) {
    Emulator emulator(TEST_DRAM_SIZE);
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.start();
    EXPECT_FALSE(emulator.finished());

    emulator.request_shutdown();
    emulator.wait();

    EXPECT_TRUE(emulator.finished());
}

TEST(EmulatorTest, GuestHaltReportsStatus) {
    Emulator emulator(TEST_DRAM_SIZE);
    load_firmware(emulator, HALT_FIRMWARE);

    emulator.run();

    EXPECT_TRUE(emulator.finished());
    EXPECT_EQ(emulator.shutdown_code(), 0);
    EXPECT_EQ(emulator.shutdown_status(), device::SiFiveTest::Status::PASS);
}

TEST(EmulatorTest, RunHonoursTheTimeout) {
    Emulator emulator(TEST_DRAM_SIZE);
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.run(std::chrono::milliseconds(50));

    EXPECT_TRUE(emulator.finished());
}

TEST(EmulatorTest, ConsoleOutputReachesTheFrontendPort) {
    Emulator emulator(TEST_DRAM_SIZE);
    load_firmware(emulator, UART_THEN_HALT_FIRMWARE);

    emulator.run();

    EXPECT_EQ(emulator.console_output(), "A");
}

TEST(EmulatorTest, StartTwiceThrows) {
    Emulator emulator(TEST_DRAM_SIZE);
    load_firmware(emulator, JIG_FIRMWARE);

    emulator.start();
    EXPECT_THROW(emulator.start(), std::logic_error);

    emulator.request_shutdown();
    emulator.wait();
}

TEST(EmulatorTest, WaitWithoutStartIsNoop) {
    Emulator emulator(TEST_DRAM_SIZE);

    EXPECT_NO_THROW(emulator.wait());
    EXPECT_FALSE(emulator.finished());
}

} // namespace uemu::test

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

#include <optional>

#include <gtest/gtest.h>

#include "device/console_channel.hpp"
#include "device/ns16550.hpp"

namespace uemu::test {

namespace {

constexpr NS16550Config UART{};
constexpr addr_t BASE = UART.base;

struct IrqRecorder {
    uint32_t id = 0;
    bool level = false;
    unsigned raised = 0;
};

} // namespace

TEST(NS16550Test, ReceiveRequiresEnabledFifo) {
    device::ConsoleChannel channel;
    device::NS16550 uart(UART, [](uint32_t, bool) {}, channel);

    // Without the FIFO the device does not consume host input at all.
    channel.push_input(0, 'w');
    uart.tick();
    EXPECT_EQ(channel.pop_input(0), std::optional<uint8_t>{'w'});

    channel.push_input(0, 'x');
    ASSERT_TRUE(uart.write<uint8_t>(BASE + device::NS16550::FCR,
                                    device::NS16550::FCR_ENABLE_FIFO));
    uart.tick();

    auto lsr = uart.read<uint8_t>(BASE + device::NS16550::LSR);
    ASSERT_TRUE(lsr.has_value());
    EXPECT_NE(*lsr & device::NS16550::LSR_DR, 0);

    auto rx = uart.read<uint8_t>(BASE + device::NS16550::RX);
    ASSERT_TRUE(rx.has_value());
    EXPECT_EQ(*rx, 'x');

    auto lsr_after = uart.read<uint8_t>(BASE + device::NS16550::LSR);
    ASSERT_TRUE(lsr_after.has_value());
    EXPECT_EQ(*lsr_after & device::NS16550::LSR_DR, 0);
}

TEST(NS16550Test, ReceiveRaisesInterrupt) {
    device::ConsoleChannel channel;
    IrqRecorder irq;
    device::NS16550 uart(
        UART,
        [&irq](uint32_t id, bool level) {
            irq.id = id;
            irq.level = level;

            if (level)
                irq.raised++;
        },
        channel);

    ASSERT_TRUE(uart.write<uint8_t>(BASE + device::NS16550::FCR,
                                    device::NS16550::FCR_ENABLE_FIFO));
    ASSERT_TRUE(uart.write<uint8_t>(BASE + device::NS16550::IER,
                                    device::NS16550::IER_RDI));

    channel.push_input(0, 'z');
    uart.tick();

    EXPECT_EQ(irq.raised, 1u);
    EXPECT_EQ(irq.id, UART.interrupt_id);
    EXPECT_TRUE(irq.level);
}

TEST(NS16550Test, TransmitUsesTheConsoleChannel) {
    device::ConsoleChannel channel;
    device::NS16550 uart(UART, [](uint32_t, bool) {}, channel);

    ASSERT_TRUE(uart.write<uint8_t>(BASE + device::NS16550::TX, 'A'));
    ASSERT_TRUE(uart.write<uint8_t>(BASE + device::NS16550::TX, 'B'));

    ASSERT_EQ(channel.port_count(), 1u);
    EXPECT_EQ(channel.port_name(0), "NS16550A");
    EXPECT_TRUE(channel.port_accepts_input(0));
    EXPECT_EQ(channel.drain_output(0), "AB");
}

} // namespace uemu::test

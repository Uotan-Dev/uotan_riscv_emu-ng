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

#include "device/console_channel.hpp"
#include "device/nemu_console.hpp"
#include "device/ns16550.hpp"

namespace uemu::test {

TEST(NemuConsoleTest, HasIndependentOutputFromNS16550) {
    constexpr NemuConsoleConfig CONSOLE{};
    constexpr NS16550Config UART{};

    device::ConsoleChannel channel;
    device::NS16550 uart(UART, [](uint32_t, bool) {}, channel);
    device::NemuConsole console(CONSOLE, channel);

    ASSERT_TRUE(uart.write<uint8_t>(UART.base + device::NS16550::TX, 'U'));

    std::string in = "NEMU";
    for (char c : in) {
        bool r = console.write(CONSOLE.base, c);
        ASSERT_TRUE(r);
    }

    ASSERT_EQ(channel.port_count(), 2u);
    EXPECT_EQ(channel.port_name(0), "NS16550A");
    EXPECT_EQ(channel.port_name(1), "NEMU Console");
    EXPECT_TRUE(channel.port_accepts_input(0));
    EXPECT_FALSE(channel.port_accepts_input(1));
    EXPECT_EQ(channel.drain_output(0), "U");
    EXPECT_EQ(channel.drain_output(1), in);
}

} // namespace uemu::test

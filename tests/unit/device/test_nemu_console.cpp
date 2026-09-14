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

namespace uemu::test {

TEST(NemuConsoleTest, BasicTest) {
    constexpr NemuConsoleConfig CONSOLE{};

    device::ConsoleChannel channel;
    device::NemuConsole console(CONSOLE, channel);

    std::string in = "Hello, uemu-ng";
    for (char c : in) {
        bool r = console.write(CONSOLE.base, c);
        ASSERT_TRUE(r);
    }

    EXPECT_EQ(channel.drain_output(), in);
}

} // namespace uemu::test

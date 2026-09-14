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

namespace uemu::test {

TEST(ConsoleChannelTest, InputRoundTrip) {
    device::ConsoleChannel channel;

    channel.push_input('a');
    channel.push_input('b');

    EXPECT_EQ(channel.pop_input(), std::optional<uint8_t>{'a'});
    EXPECT_EQ(channel.pop_input(), std::optional<uint8_t>{'b'});
    EXPECT_EQ(channel.pop_input(), std::nullopt);
}

TEST(ConsoleChannelTest, OutputDrainEmptiesTheQueue) {
    device::ConsoleChannel channel;

    channel.push_output('x');
    channel.push_output('y');

    EXPECT_EQ(channel.drain_output(), "xy");
    EXPECT_TRUE(channel.drain_output().empty());
}

TEST(ConsoleChannelTest, FullQueueDropsNewestByte) {
    device::ConsoleChannel channel;

    for (size_t i = 0; i < device::ConsoleChannel::CAPACITY + 16; i++)
        channel.push_output(static_cast<uint8_t>(i));

    std::string bytes = channel.drain_output();

    ASSERT_EQ(bytes.size(), device::ConsoleChannel::CAPACITY);
    EXPECT_EQ(static_cast<uint8_t>(bytes.front()), 0);
    EXPECT_EQ(static_cast<uint8_t>(bytes.back()),
              static_cast<uint8_t>(device::ConsoleChannel::CAPACITY - 1));
}

} // namespace uemu::test

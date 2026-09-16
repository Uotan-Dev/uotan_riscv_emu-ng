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
    auto input = channel.register_port("input", true);
    auto output_only = channel.register_port("output", false);

    channel.push_input('a');
    channel.push_input('b');

    EXPECT_EQ(channel.pop_input(output_only), std::nullopt);
    EXPECT_EQ(channel.pop_input(input), std::optional<uint8_t>{'a'});
    EXPECT_EQ(channel.pop_input(input), std::optional<uint8_t>{'b'});
    EXPECT_EQ(channel.pop_input(input), std::nullopt);
}

TEST(ConsoleChannelTest, OutputsAreNamedAndIndependent) {
    device::ConsoleChannel channel;
    auto first = channel.register_port("first", true);
    auto second = channel.register_port("second", false);

    channel.push_output(first, 'a');
    channel.push_output(second, 'x');
    channel.push_output(first, 'b');
    channel.push_output(second, 'y');

    EXPECT_EQ(channel.output_count(), 2u);
    EXPECT_EQ(channel.output_name(first), "first");
    EXPECT_EQ(channel.output_name(second), "second");
    EXPECT_EQ(channel.drain_output(first), "ab");
    EXPECT_EQ(channel.drain_output(second), "xy");
    EXPECT_TRUE(channel.drain_output(first).empty());
    EXPECT_TRUE(channel.drain_output(second).empty());
}

TEST(ConsoleChannelTest, RejectsASecondInputPort) {
    device::ConsoleChannel channel;
    static_cast<void>(channel.register_port("first", true));

    EXPECT_THROW(static_cast<void>(channel.register_port("second", true)),
                 std::logic_error);
    EXPECT_EQ(channel.output_count(), 1u);
}

TEST(ConsoleChannelTest, FullQueuesDropNewestByteIndependently) {
    device::ConsoleChannel channel;
    auto first = channel.register_port("first", true);
    auto second = channel.register_port("second", false);

    for (size_t i = 0; i < device::ConsoleChannel::CAPACITY + 16; i++) {
        channel.push_input(static_cast<uint8_t>(i));
        channel.push_output(first, static_cast<uint8_t>(i));
    }
    channel.push_output(second, 'z');

    std::string bytes = channel.drain_output(first);

    ASSERT_EQ(bytes.size(), device::ConsoleChannel::CAPACITY);
    EXPECT_EQ(static_cast<uint8_t>(bytes.front()), 0);
    EXPECT_EQ(static_cast<uint8_t>(bytes.back()),
              static_cast<uint8_t>(device::ConsoleChannel::CAPACITY - 1));
    EXPECT_EQ(channel.drain_output(second), "z");

    for (size_t i = 0; i < device::ConsoleChannel::CAPACITY; i++)
        EXPECT_EQ(channel.pop_input(first),
                  std::optional<uint8_t>{static_cast<uint8_t>(i)});
    EXPECT_EQ(channel.pop_input(first), std::nullopt);
}

} // namespace uemu::test

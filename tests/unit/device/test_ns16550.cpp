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
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "device/ns16550.hpp"

namespace uemu::test {

class NS16550Test : public ::testing::Test {
protected:
    void SetUp() override {
        irq_events.clear();
        uart =
            std::make_shared<device::NS16550>([this](uint32_t id, bool level) {
                irq_events.emplace_back(id, level);
            });
    }

    addr_t reg(uint8_t offset) const {
        return device::NS16550::DEFAULT_BASE + offset;
    }

    std::shared_ptr<device::NS16550> uart;
    std::vector<std::pair<uint32_t, bool>> irq_events;
};

TEST_F(NS16550Test, PushBytesCanBeReadFromRxRegister) {
    EXPECT_TRUE(uart->write<uint8_t>(reg(device::NS16550::IER),
                                     device::NS16550::IER_RDI));

    const std::array<uint8_t, 3> input{'a', 'b', 'c'};
    EXPECT_EQ(uart->push_bytes(input), input.size());

    auto lsr = uart->read<uint8_t>(reg(device::NS16550::LSR));
    ASSERT_TRUE(lsr.has_value());
    EXPECT_TRUE(*lsr & device::NS16550::LSR_DR);
    ASSERT_FALSE(irq_events.empty());
    EXPECT_TRUE(irq_events.back().second);

    for (uint8_t expected : input) {
        auto byte = uart->read<uint8_t>(reg(device::NS16550::RX));
        ASSERT_TRUE(byte.has_value());
        EXPECT_EQ(*byte, expected);
    }

    lsr = uart->read<uint8_t>(reg(device::NS16550::LSR));
    ASSERT_TRUE(lsr.has_value());
    EXPECT_FALSE(*lsr & device::NS16550::LSR_DR);
    ASSERT_FALSE(irq_events.empty());
    EXPECT_FALSE(irq_events.back().second);
}

TEST_F(NS16550Test, PushBytesStopsWhenRxQueueIsFull) {
    std::array<uint8_t, device::NS16550::QUEUE_SIZE + 3> input{};
    for (size_t i = 0; i < input.size(); i++)
        input[i] = static_cast<uint8_t>(i);

    EXPECT_EQ(uart->push_bytes(input), device::NS16550::QUEUE_SIZE);

    for (size_t i = 0; i < device::NS16550::QUEUE_SIZE; i++) {
        auto byte = uart->read<uint8_t>(reg(device::NS16550::RX));
        ASSERT_TRUE(byte.has_value());
        EXPECT_EQ(*byte, input[i]);
    }

    auto lsr = uart->read<uint8_t>(reg(device::NS16550::LSR));
    ASSERT_TRUE(lsr.has_value());
    EXPECT_FALSE(*lsr & device::NS16550::LSR_DR);
}

TEST_F(NS16550Test, TxRegisterCanBePoppedFromByteSource) {
    EXPECT_TRUE(uart->write<uint8_t>(reg(device::NS16550::TX), 'x'));
    EXPECT_TRUE(uart->write<uint8_t>(reg(device::NS16550::TX), 'y'));

    std::array<uint8_t, 4> output{};
    EXPECT_EQ(uart->pop_bytes(output), 2);
    EXPECT_EQ(output[0], 'x');
    EXPECT_EQ(output[1], 'y');
    EXPECT_EQ(uart->pop_bytes(output), 0);
}

TEST_F(NS16550Test, LoopbackDoesNotEnterTxByteSource) {
    EXPECT_TRUE(uart->write<uint8_t>(reg(device::NS16550::MCR),
                                     device::NS16550::MCR_LOOP));
    EXPECT_TRUE(uart->write<uint8_t>(reg(device::NS16550::TX), 'z'));

    std::array<uint8_t, 1> output{};
    EXPECT_EQ(uart->pop_bytes(output), 0);

    auto byte = uart->read<uint8_t>(reg(device::NS16550::RX));
    ASSERT_TRUE(byte.has_value());
    EXPECT_EQ(*byte, 'z');
}

} // namespace uemu::test

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

#include "core/input.hpp"
#include "device/goldfish_events.hpp"

extern "C" {
#include "linux/input-event-codes.h" // IWYU pragma: keep
}

namespace uemu::test {

namespace {

constexpr addr_t BASE = device::GoldfishEvents::DEFAULT_BASE;

struct IrqRecorder {
    bool level = false;
    unsigned count = 0;
};

} // namespace

TEST(GoldfishEventsTest, KeyEventsBecomeInputTriples) {
    device::GoldfishEvents events([](uint32_t, bool) {});

    events.push_key_event(
        {.code = KEY_A, .action = core::KeyEvent::Action::Press});
    events.push_key_event(
        {.code = KEY_A, .action = core::KeyEvent::Action::Release});

    // Each event is queued as (type, code, value).
    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{EV_KEY});
    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{KEY_A});
    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{1});

    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{EV_KEY});
    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{KEY_A});
    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{0});

    EXPECT_EQ(events.read<uint32_t>(BASE + device::GoldfishEvents::REG_READ),
              std::optional<uint32_t>{0});
}

TEST(GoldfishEventsTest, BufferedEventsRaiseIrqWhenTheGuestIsReady) {
    IrqRecorder irq;
    device::GoldfishEvents events([&irq](uint32_t, bool level) {
        irq.level = level;
        irq.count++;
    });

    events.push_key_event(
        {.code = KEY_A, .action = core::KeyEvent::Action::Press});
    EXPECT_EQ(irq.count, 0u);

    ASSERT_TRUE(
        events.write<uint32_t>(BASE + device::GoldfishEvents::REG_SET_PAGE,
                               device::GoldfishEvents::PAGE_ABSDATA));
    (void)events.read<uint32_t>(BASE + device::GoldfishEvents::REG_LEN);

    EXPECT_EQ(irq.count, 1u);
    EXPECT_TRUE(irq.level);
}

} // namespace uemu::test

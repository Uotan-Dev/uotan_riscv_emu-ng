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

#include "time_source.hpp"

namespace uemu::test {

TEST(TimeSourceTest, DividesSimulationStepsWithoutLosingRemainder) {
    TimerStepDivider divider;

    EXPECT_EQ(divider.advance(99), 0);
    EXPECT_EQ(divider.advance(1), 1);
    EXPECT_EQ(divider.advance(250), 2);
    EXPECT_EQ(divider.advance(49), 0);
    EXPECT_EQ(divider.advance(1), 1);
    EXPECT_EQ(divider.advance(5000), 50);
}

TEST(TimeSourceTest, DeterministicSourceAdvancesFromWrittenValue) {
    auto source = make_time_source(TimerMode::Deterministic, 10'000'000);

    EXPECT_TRUE(source->is_deterministic());
    EXPECT_EQ(source->read(), 0);

    source->advance(3);
    EXPECT_EQ(source->read(), 3);

    source->write(42);
    source->advance(1);
    EXPECT_EQ(source->read(), 43);
}

} // namespace uemu::test

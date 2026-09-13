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

#pragma once

#include <cstdint>
#include <memory>

namespace uemu {

enum class TimerMode : uint8_t {
    Realtime,
    Deterministic,
};

class TimerStepDivider {
public:
    static constexpr uint64_t STEPS_PER_TICK = 100;

    [[nodiscard]] uint64_t advance(uint64_t steps) noexcept {
        const uint64_t steps_to_tick = STEPS_PER_TICK - remainder_;
        if (steps < steps_to_tick) {
            remainder_ += steps;
            return 0;
        }

        steps -= steps_to_tick;
        const uint64_t ticks = 1 + steps / STEPS_PER_TICK;
        remainder_ = steps % STEPS_PER_TICK;
        return ticks;
    }

private:
    uint64_t remainder_ = 0;
};

class TimeSource {
public:
    virtual ~TimeSource() = default;

    [[nodiscard]] virtual uint64_t read() const noexcept = 0;
    virtual void write(uint64_t value) noexcept = 0;
    virtual void advance(uint64_t ticks) noexcept = 0;
    [[nodiscard]] virtual bool is_deterministic() const noexcept = 0;
};

[[nodiscard]] std::unique_ptr<TimeSource> make_time_source(TimerMode mode,
                                                           uint64_t frequency);

} // namespace uemu

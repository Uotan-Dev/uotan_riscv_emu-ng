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

#include "time_source.hpp"

#include <chrono>

namespace uemu {
namespace {

class RealtimeTimeSource final : public TimeSource {
public:
    explicit RealtimeTimeSource(uint64_t frequency)
        : frequency_(frequency), base_time_(std::chrono::steady_clock::now()) {}

    [[nodiscard]] uint64_t read() const noexcept override {
        const auto elapsed = std::chrono::steady_clock::now() - base_time_;
        const auto seconds = std::chrono::duration<double>(elapsed).count();
        return base_value_ +
               static_cast<uint64_t>(seconds * static_cast<double>(frequency_));
    }

    void write(uint64_t value) noexcept override {
        base_value_ = value;
        base_time_ = std::chrono::steady_clock::now();
    }

    void advance(uint64_t) noexcept override {}

    [[nodiscard]] bool is_deterministic() const noexcept override {
        return false;
    }

private:
    const uint64_t frequency_;
    uint64_t base_value_ = 0;
    std::chrono::steady_clock::time_point base_time_;
};

class DeterministicTimeSource final : public TimeSource {
public:
    [[nodiscard]] uint64_t read() const noexcept override { return value_; }

    void write(uint64_t value) noexcept override { value_ = value; }

    void advance(uint64_t ticks) noexcept override { value_ += ticks; }

    [[nodiscard]] bool is_deterministic() const noexcept override {
        return true;
    }

private:
    uint64_t value_ = 0;
};

} // namespace

std::unique_ptr<TimeSource> make_time_source(TimerMode mode,
                                             uint64_t frequency) {
    if (mode == TimerMode::Deterministic)
        return std::make_unique<DeterministicTimeSource>();

    return std::make_unique<RealtimeTimeSource>(frequency);
}

} // namespace uemu

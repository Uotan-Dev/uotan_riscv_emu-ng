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

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

namespace uemu::device {

// Bounded byte channel between the guest console devices and the host
// frontend.  It carries no host I/O and knows nothing about terminals, files
// or windows: the frontend pushes host input bytes and drains guest output
// bytes, console devices do the opposite.
//
// Thread-safe; a full queue drops the newest byte so guest output can never
// grow without bound when the frontend stalls.
class ConsoleChannel {
public:
    static constexpr size_t CAPACITY = 4096;

    // Host frontend -> guest console device.
    void push_input(uint8_t byte) {
        std::scoped_lock lock(mutex_);

        if (input_.size() < CAPACITY)
            input_.push_back(byte);
    }

    [[nodiscard]] std::optional<uint8_t> pop_input() {
        std::scoped_lock lock(mutex_);

        if (input_.empty())
            return std::nullopt;

        uint8_t byte = input_.front();
        input_.pop_front();
        return byte;
    }

    // Guest console device -> host frontend.
    void push_output(uint8_t byte) {
        std::scoped_lock lock(mutex_);

        if (output_.size() < CAPACITY)
            output_.push_back(byte);
    }

    [[nodiscard]] std::string drain_output() {
        std::scoped_lock lock(mutex_);

        std::string bytes;
        bytes.reserve(output_.size());

        for (uint8_t byte : output_)
            bytes.push_back(static_cast<char>(byte));

        output_.clear();
        return bytes;
    }

private:
    std::mutex mutex_;
    std::deque<uint8_t> input_;
    std::deque<uint8_t> output_;
};

} // namespace uemu::device

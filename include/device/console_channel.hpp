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
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    using Port = size_t;

    static constexpr size_t CAPACITY = 4096;

    [[nodiscard]] Port register_port(std::string name, bool accepts_input) {
        std::scoped_lock lock(mutex_);

        if (accepts_input && input_port_.has_value())
            throw std::logic_error(
                "a console input port is already registered");

        const Port port = outputs_.size();
        outputs_.push_back({.name = std::move(name), .queue = {}});

        if (accepts_input)
            input_port_ = port;

        return port;
    }

    // Host frontend -> guest console device.
    void push_input(uint8_t byte) {
        std::scoped_lock lock(mutex_);

        if (input_.size() < CAPACITY)
            input_.push_back(byte);
    }

    [[nodiscard]] std::optional<uint8_t> pop_input(Port port) {
        std::scoped_lock lock(mutex_);

        static_cast<void>(outputs_.at(port));
        if (input_port_ != port)
            return std::nullopt;

        if (input_.empty())
            return std::nullopt;

        uint8_t byte = input_.front();
        input_.pop_front();
        return byte;
    }

    // Guest console device -> host frontend.
    void push_output(Port port, uint8_t byte) {
        std::scoped_lock lock(mutex_);

        Output& output = outputs_.at(port);

        if (output.queue.size() < CAPACITY)
            output.queue.push_back(byte);
    }

    [[nodiscard]] size_t output_count() const {
        std::scoped_lock lock(mutex_);
        return outputs_.size();
    }

    [[nodiscard]] std::string output_name(Port port) const {
        std::scoped_lock lock(mutex_);
        return outputs_.at(port).name;
    }

    [[nodiscard]] std::string drain_output(Port port) {
        std::scoped_lock lock(mutex_);

        std::deque<uint8_t>& queue = outputs_.at(port).queue;

        std::string bytes;
        bytes.reserve(queue.size());

        for (uint8_t byte : queue)
            bytes.push_back(static_cast<char>(byte));

        queue.clear();
        return bytes;
    }

private:
    struct Output {
        std::string name;
        std::deque<uint8_t> queue;
    };

    mutable std::mutex mutex_;
    std::deque<uint8_t> input_;
    std::vector<Output> outputs_;
    std::optional<Port> input_port_;
};

} // namespace uemu::device

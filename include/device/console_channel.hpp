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

        const Port port = ports_.size();
        ports_.push_back({.name = std::move(name),
                          .accepts_input = accepts_input,
                          .input = {},
                          .output = {}});

        return port;
    }

    // Host frontend -> guest console device.
    void push_input(Port port, uint8_t byte) {
        std::scoped_lock lock(mutex_);

        PortState& state = ports_.at(port);

        if (state.accepts_input && state.input.size() < CAPACITY)
            state.input.push_back(byte);
    }

    [[nodiscard]] std::optional<uint8_t> pop_input(Port port) {
        std::scoped_lock lock(mutex_);

        PortState& state = ports_.at(port);

        if (!state.accepts_input || state.input.empty())
            return std::nullopt;

        uint8_t byte = state.input.front();
        state.input.pop_front();
        return byte;
    }

    // Guest console device -> host frontend.
    void push_output(Port port, uint8_t byte) {
        std::scoped_lock lock(mutex_);

        std::deque<uint8_t>& output = ports_.at(port).output;

        if (output.size() < CAPACITY)
            output.push_back(byte);
    }

    [[nodiscard]] size_t port_count() const {
        std::scoped_lock lock(mutex_);
        return ports_.size();
    }

    [[nodiscard]] std::string port_name(Port port) const {
        std::scoped_lock lock(mutex_);
        return ports_.at(port).name;
    }

    [[nodiscard]] bool port_accepts_input(Port port) const {
        std::scoped_lock lock(mutex_);
        return ports_.at(port).accepts_input;
    }

    [[nodiscard]] std::string drain_output(Port port) {
        std::scoped_lock lock(mutex_);

        std::deque<uint8_t>& queue = ports_.at(port).output;

        std::string bytes;
        bytes.reserve(queue.size());

        for (uint8_t byte : queue)
            bytes.push_back(static_cast<char>(byte));

        queue.clear();
        return bytes;
    }

private:
    struct PortState {
        std::string name;
        bool accepts_input;
        std::deque<uint8_t> input;
        std::deque<uint8_t> output;
    };

    mutable std::mutex mutex_;
    std::vector<PortState> ports_;
};

} // namespace uemu::device

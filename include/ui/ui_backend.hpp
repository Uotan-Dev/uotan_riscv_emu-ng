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

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "ui/byte_sink.hpp"
#include "ui/byte_source.hpp"
#include "ui/input_sink.hpp"
#include "ui/pixel_source.hpp"
#include "ui/terminal.hpp"

namespace uemu::ui {

class UIBackend {
public:
    using ExitCallback = std::function<void(void)>;

    struct Endpoints {
        std::shared_ptr<ui::PixelSource> pixel_source;
        std::shared_ptr<ui::InputSink> input_sink;
        std::shared_ptr<ui::ByteSink> byte_sink;
        std::shared_ptr<ui::ByteSource> byte_source;
    };

    UIBackend(Endpoints endpoints, ExitCallback exit_callback)
        : endpoints_(std::move(endpoints)),
          exit_callback_(std::move(exit_callback)) {}

    virtual ~UIBackend() = default;
    virtual void update() = 0;

protected:
    void request_exit() {
        if (exit_callback_)
            exit_callback_();
    }

    void pump_terminal_io() {
        if (endpoints_.byte_sink) {
            while (std::optional<uint8_t> byte = terminal::read_byte())
                pending_terminal_input_.push_back(*byte);

            while (!pending_terminal_input_.empty()) {
                std::array<uint8_t, 64> buffer{};
                const size_t size =
                    std::min(buffer.size(), pending_terminal_input_.size());

                for (size_t i = 0; i < size; i++)
                    buffer[i] = pending_terminal_input_[i];

                size_t accepted = endpoints_.byte_sink->push_bytes(
                    std::span<const uint8_t>(buffer.data(), size));

                if (accepted == 0)
                    break;

                for (size_t i = 0; i < accepted; i++)
                    pending_terminal_input_.pop_front();
            }
        }

        if (endpoints_.byte_source) {
            std::array<uint8_t, 256> buffer{};
            size_t size = 0;

            do {
                size = endpoints_.byte_source->pop_bytes(buffer);
                for (size_t i = 0; i < size; i++)
                    terminal::write_byte(buffer[i]);
            } while (size == buffer.size());
        }
    }

    Endpoints endpoints_;

private:
    ExitCallback exit_callback_;
    std::deque<uint8_t> pending_terminal_input_;
};

} // namespace uemu::ui

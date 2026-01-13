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

#include <functional>
#include <memory>

#include "ui/input_sink.hpp"
#include "ui/pixel_source.hpp"

namespace uemu::ui {

class UIBackend {
public:
    using ExitCallback = std::function<void(void)>;

    UIBackend(std::shared_ptr<ui::PixelSource> pixel_source,
              std::shared_ptr<ui::InputSink> input_sink,
              ExitCallback exit_callback)
        : pixel_source_(std::move(pixel_source)),
          input_sink_(std::move(input_sink)), exit_callback_(exit_callback) {}

    virtual ~UIBackend() = default;
    virtual void update() = 0;

protected:
    void request_exit() {
        if (exit_callback_)
            exit_callback_();
    }

    std::shared_ptr<ui::PixelSource> pixel_source_;
    std::shared_ptr<ui::InputSink> input_sink_;

private:
    ExitCallback exit_callback_;
};

} // namespace uemu::ui

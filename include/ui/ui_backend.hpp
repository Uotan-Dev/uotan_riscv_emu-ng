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
#include <stdexcept>

#include "ui/console_endpoint.hpp"
#include "ui/input_sink.hpp"
#include "ui/pixel_source.hpp"

namespace uemu::ui {

class UIBackend {
public:
    using ExitCallback = std::function<void(void)>;

    struct Endpoints {
        std::shared_ptr<ui::ConsoleEndpoint> console_endpoint;
        std::shared_ptr<ui::InputSink> input_sink;
        std::shared_ptr<ui::PixelSource> pixel_source;
        ExitCallback exit_callback;
    };

    UIBackend(Endpoints endpoints) : endpoints_(std::move(endpoints)) {
        if (initialized_)
            throw std::runtime_error("Only one UIBackend instance is allowed.");

        initialized_ = true;
    }

    virtual ~UIBackend() { initialized_ = false; }

    virtual void update() = 0;

protected:
    void request_exit() const {
        if (endpoints_.exit_callback)
            endpoints_.exit_callback();
    }

    Endpoints endpoints_;

private:
    static bool initialized_;
};

} // namespace uemu::ui

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

#include <memory>

#include "device/simple_fb.hpp"

namespace uemu::host::ui {

class UIBackend {
public:
    using HaltCallback = std::function<void(void)>;

    UIBackend(std::shared_ptr<device::SimpleFB> display,
              HaltCallback halt_callback)
        : display_(std::move(display)), halt_callback_(halt_callback) {}

    virtual ~UIBackend() = default;

    virtual void update() = 0;

protected:
    void request_halt() {
        if (halt_callback_)
            halt_callback_();
    }

    std::shared_ptr<device::SimpleFB> display_;
    HaltCallback halt_callback_;
};

} // namespace uemu::host::ui

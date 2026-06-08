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

#include <thread>

#include "ui/ui_backend.hpp"

namespace uemu::ui {

class HeadlessBackend : public UIBackend {
public:
    HeadlessBackend() : UIBackend() {
        terminal::enable_raw_mode();
    }

    ~HeadlessBackend() override { terminal::restore_mode(); }

    void run(std::function<bool(void)> should_continue) override {
        while (running_ && should_continue()) {
            update();
            std::this_thread::yield();
        }
    }

protected:
    void update() override { pump_terminal_io(); }
};

} // namespace uemu::ui

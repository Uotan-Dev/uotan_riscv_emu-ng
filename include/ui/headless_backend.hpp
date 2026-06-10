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

#include "ui/host_console.hpp"
#include "ui/ui_backend.hpp"

namespace uemu::ui {

class HeadlessBackend : public UIBackend {
public:
    HeadlessBackend(Endpoints endpoints) : UIBackend(std::move(endpoints)) {
        HostConsole::apply_to_endpoint(*endpoints_.console_endpoint);
    }

    void update() override {}

private:
    HostConsole host_console_;
};

} // namespace uemu::ui

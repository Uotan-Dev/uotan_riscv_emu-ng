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

#include <chrono>
#include <memory>

#include "frontend/frontend.hpp"

namespace uemu::frontend {

// GTK4/libadwaita frontend.  Its implementation is kept out of this header so
// GTK remains a private dependency of the frontend library.
class Gtk4Frontend final : public Frontend {
public:
    explicit Gtk4Frontend(Emulator& emulator);
    ~Gtk4Frontend() override;

    void run(std::chrono::milliseconds timeout =
                 std::chrono::milliseconds::zero()) override;

private:
    class Impl;

    // Gtk4Frontend uses the GLib main loop instead of Frontend's polling loop.
    void poll_input() override;
    void present() override;

    std::unique_ptr<Impl> impl_;
};

} // namespace uemu::frontend

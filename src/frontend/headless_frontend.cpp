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

#include <cstddef>
#include <string>

#include "common/log.hpp"
#include "emulator.hpp"
#include "frontend/headless_frontend.hpp"

namespace uemu::frontend {

HeadlessFrontend::HeadlessFrontend(Emulator& emulator) : Frontend(emulator) {
    for (size_t console = 0; console < emulator_.console_count(); console++) {
        if (!emulator_.console_accepts_input(console))
            continue;

        if (input_console_.has_value()) {
            log::warn(
                "multiple input consoles are available; headless frontend "
                "uses \"{}\" for host input",
                emulator_.console_name(*input_console_));
            break;
        }

        input_console_ = console;
    }
}

void HeadlessFrontend::poll_input() {
    if (!input_console_.has_value())
        return;

    if (std::string bytes = terminal_.read_input(); !bytes.empty())
        emulator_.console_input(*input_console_, bytes);
}

void HeadlessFrontend::present() {
    for (size_t console = 0; console < emulator_.console_count(); console++) {
        if (std::string bytes = emulator_.console_output(console);
            !bytes.empty())
            terminal_.write(bytes);
    }
}

} // namespace uemu::frontend

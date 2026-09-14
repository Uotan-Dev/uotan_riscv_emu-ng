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

#include <string>

#include "emulator.hpp"
#include "frontend/headless_frontend.hpp"

namespace uemu::frontend {

HeadlessFrontend::HeadlessFrontend(Emulator& emulator) : Frontend(emulator) {}

void HeadlessFrontend::poll_input() {
    if (std::string bytes = terminal_.read_input(); !bytes.empty())
        emulator_.console_input(bytes);
}

void HeadlessFrontend::present() {
    if (std::string bytes = emulator_.console_output(); !bytes.empty())
        terminal_.write(bytes);
}

} // namespace uemu::frontend

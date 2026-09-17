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
#include <optional>

#include "frontend/frontend.hpp"
#include "frontend/terminal.hpp"

namespace uemu::frontend {

// Terminal-only frontend: host stdin feeds the guest console and guest console
// output is written to host stdout.  This is the --headless mode used by the
// ACT4 and riscv-tests runners.
class HeadlessFrontend final : public Frontend {
public:
    explicit HeadlessFrontend(Emulator& emulator);

protected:
    void poll_input() override;
    void present() override;

private:
    Terminal terminal_;
    std::optional<size_t> input_console_;
};

} // namespace uemu::frontend

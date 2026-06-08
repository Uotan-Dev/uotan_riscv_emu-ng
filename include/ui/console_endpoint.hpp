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
#include <optional>

namespace uemu::ui {

// Mixin that wires host-side console I/O callbacks into a device.
// Devices (e.g. NS16550) inherit from this and call read_char / write_char;
// the UI backend sets the callbacks before the CPU thread starts.
//
// This class is not meant to be instantiated on its own — only via
// inheritance. The protected constructor enforces that.
class ConsoleEndpoint {
public:
    /// Non-blocking read: returns a character, or std::nullopt if none
    /// available.
    std::function<std::optional<char>(void)> read_char{};

    /// Write a single character to the console output.
    std::function<void(char)> write_char{};

protected:
    ConsoleEndpoint() = default;
    ~ConsoleEndpoint() = default;
};

} // namespace uemu::ui

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

#include <cstdint>

namespace uemu::core {

// A host keyboard event.  `code` uses the Linux input-event-code space; the
// concrete frontends translate their own key codes (for example SDL
// scancodes) into it.
struct KeyEvent {
    enum class Action : uint8_t { Press, Release };

    uint32_t code;
    Action action;
};

} // namespace uemu::core

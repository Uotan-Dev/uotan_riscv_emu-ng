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

namespace uemu {

class Emulator;

} // namespace uemu

namespace uemu::frontend {

// Host frontend, owned and run by the main thread.  It is the only place that
// touches host resources (terminal, window, input devices) and it drives the
// emulator lifecycle while the guest runs.  The emulator core never references
// this class: it only exposes small ports and a start/wait lifecycle, which
// subclasses reach through the Emulator reference they are built with.
class Frontend {
public:
    virtual ~Frontend() = default;

    Frontend(const Frontend&) = delete;
    Frontend& operator=(const Frontend&) = delete;
    Frontend(Frontend&&) = delete;
    Frontend& operator=(Frontend&&) = delete;

    // Runs the main loop: polls host input, presents guest output and stops on
    // guest halt, host quit or the optional timeout.  Rethrows worker
    // exceptions and leaves the emulator stopped and joined.
    void
    run(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

protected:
    explicit Frontend(Emulator& emulator) : emulator_(emulator) {}

    // Forward host events to the emulator (stdin bytes, key events, quit).
    virtual void poll_input() = 0;

    // Push guest output to the host (console bytes, framebuffer).
    virtual void present() = 0;

    Emulator& emulator_;
};

} // namespace uemu::frontend

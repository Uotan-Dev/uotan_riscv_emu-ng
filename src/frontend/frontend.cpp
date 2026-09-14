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

#include <chrono>
#include <exception>
#include <thread>

#include "common/log.hpp"
#include "emulator.hpp"
#include "frontend/frontend.hpp"

namespace uemu::frontend {

void Frontend::run(std::chrono::milliseconds timeout) {
    emulator_.start();

    const bool timed = timeout.count() > 0;
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!emulator_.finished()) {
        if (timed && std::chrono::steady_clock::now() >= deadline) {
            log::warn("Execution timeout reached ({} ms), shutting down...",
                      timeout.count());
            break;
        }

        poll_input();
        present();

        // Frontend work is event driven; the CPU and device threads keep the
        // guest and the devices busy in the meantime.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    emulator_.request_shutdown();

    // Join the workers first, then flush what the guest wrote before it
    // halted: harnesses such as ACT4 parse the last console line, which may
    // have been produced after the last loop iteration.  A worker exception is
    // reported after the flush so no output is lost on the error path either.
    std::exception_ptr error;

    try {
        emulator_.wait();
    } catch (...) { error = std::current_exception(); }

    present();

    if (error)
        std::rethrow_exception(error);

    // The frontend owns user-facing reporting of the run outcome.
    log::info("Emulator shutdown with code 0x{:x} and status 0x{:x}",
              emulator_.shutdown_code(), emulator_.shutdown_status());
}

} // namespace uemu::frontend

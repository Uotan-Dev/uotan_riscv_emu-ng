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

#include <exception>
#include <stop_token>
#include <thread>

#include "core/hart.hpp"
#include "core/mmu.hpp"

namespace uemu::core {

// Owns the CPU thread, which fetches, decodes and executes guest instructions.
// The execution loop is self-contained: it performs no host I/O and knows
// nothing about the frontend.  One instance per hart.
class Cpu {
public:
    Cpu(Hart& hart, MMU& mmu, std::stop_source stop_source);

    ~Cpu();

    Cpu(const Cpu&) = delete;
    Cpu& operator=(const Cpu&) = delete;
    Cpu(Cpu&&) = delete;
    Cpu& operator=(Cpu&&) = delete;

    // Starts the CPU thread.  Throws std::logic_error when called twice.
    void start();

    // Joins the CPU thread when it was started.
    void join();

    [[nodiscard]] std::exception_ptr exception() const noexcept {
        return exception_;
    }

private:
    void run();

    Hart& hart_;
    MMU& mmu_;

    std::stop_source stop_source_;

    MCYCLE* mcycle_;
    MINSTRET* minstret_;

    std::thread thread_;
    std::exception_ptr exception_;
};

} // namespace uemu::core

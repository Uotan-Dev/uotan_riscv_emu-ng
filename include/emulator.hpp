/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

#include "board_config.hpp"
#include "common/types.hpp"
#include "core/cpu.hpp"
#include "core/device_thread.hpp"
#include "core/framebuffer.hpp"
#include "core/input.hpp"
#include "device/console_channel.hpp"

namespace uemu::device {

class GoldfishEvents;
class NS16550;
class SimpleFB;

} // namespace uemu::device

namespace uemu {

// The emulated machine: hart, memory, MMU, bus and devices, plus the two
// worker threads that drive them.
//
// The board it builds is described by `config` (see board_config.hpp), which is
// the single source of truth for the address map and the device parameters.
//
// Thread ownership:
//   * CPU thread    - core::Cpu, guest instruction execution.
//   * device thread - core::DeviceThread, device progress.
//   * main thread   - construction, image loading, lifecycle and every
//                     frontend port below (console bytes, key events,
//                     framebuffer).
//
// The guest image must be loaded before start(), and an Emulator instance is
// single-use.
class Emulator {
public:
    explicit Emulator(const BoardConfig& config);
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    Emulator(Emulator&&) = delete;
    Emulator& operator=(Emulator&&) = delete;

    // Load an elf from path to DRAM
    void loadelf(const std::filesystem::path& path);

    // Load data from p to DRAM
    void load(addr_t addr, const void* p, size_t n);

    // Load a common file to DRAM
    void load(addr_t addr, const std::filesystem::path& path);

    // Load data from a vector<T> to DRAM
    template <typename T>
    void load(addr_t addr, const std::vector<T>& data) {
        if (!data.empty())
            load(addr, data.data(), sizeof(T) * data.size());
    }

    // Starts the CPU and device threads.  Throws std::logic_error when the
    // emulator has already been started.
    void start();

    // Convenience for frontend-less runs (unit and ISA tests): start the
    // workers and block until the guest halts or the timeout expires.
    void
    run(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    // Asks the workers to stop.  Safe to call from any thread at any time.
    void request_shutdown() noexcept;

    // True once a stop was requested: guest halt, host quit or timeout.
    [[nodiscard]] bool finished() const noexcept;

    // Joins both workers and rethrows an exception they reported.
    void wait();

    // Valid once the run has finished: the guest-halt path writes them before
    // requesting the stop, so read them after wait() (or once finished()).
    [[nodiscard]] uint16_t shutdown_code() const noexcept {
        return shutdown_code_;
    }

    [[nodiscard]] uint16_t shutdown_status() const noexcept {
        return shutdown_status_;
    }

    // Frontend ports.  Main thread only.
    void console_input(std::string_view bytes);
    [[nodiscard]] std::string console_output();
    void push_key_event(core::KeyEvent event);
    [[nodiscard]] core::Framebuffer& framebuffer() noexcept;

private:
    void halt_from_guest(uint16_t code, uint16_t status) noexcept;

    // Declared in dependency order: reverse destruction joins the workers
    // before the objects they reference go away.
    std::shared_ptr<core::Dram> dram_;
    std::shared_ptr<core::Hart> hart_;
    std::shared_ptr<core::Bus> bus_;
    std::shared_ptr<core::MMU> mmu_;

    device::ConsoleChannel console_channel_;
    std::shared_ptr<device::NS16550> console_;
    std::shared_ptr<device::GoldfishEvents> input_;
    std::shared_ptr<device::SimpleFB> framebuffer_;

    std::stop_source stop_source_;
    bool started_ = false;
    uint16_t shutdown_code_ = 0;
    uint16_t shutdown_status_ = 0;

    core::Cpu cpu_;
    core::DeviceThread device_thread_;
};

} // namespace uemu

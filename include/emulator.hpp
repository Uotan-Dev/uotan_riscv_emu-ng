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

#include <condition_variable>
#include <filesystem>
#include <thread>

#include "core/mmu.hpp"

namespace uemu {

class Emulator {
public:
    explicit Emulator(size_t dram_size);
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    Emulator(Emulator&&) = delete;
    Emulator& operator=(Emulator&&) = delete;

    void run();
    void stop();

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

    uint16_t shutdown_code() const noexcept { return shutdown_code_; }

    uint16_t shutdown_status() const noexcept { return shutdown_status_; }

private:
    void cpu_thread();

    std::shared_ptr<core::Hart> hart_;
    std::shared_ptr<core::Dram> dram_;
    std::shared_ptr<core::Bus> bus_;
    std::shared_ptr<core::MMU> mmu_;

    bool cpu_thread_running_;
    std::unique_ptr<std::thread> cpu_thread_;
    std::mutex cpu_mutex_;
    std::condition_variable cpu_cond_;
    std::exception_ptr cpu_thread_exception_;

    bool shutdown_;
    uint16_t shutdown_code_;
    uint16_t shutdown_status_;
};

} // namespace uemu

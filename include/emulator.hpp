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

#include <filesystem>

#include "executionengine.hpp"
#include "host/console.hpp"

namespace uemu {

class Emulator {
public:
    explicit Emulator(size_t dram_size, bool headless = true);
    ~Emulator() = default;

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    Emulator(Emulator&&) = delete;
    Emulator& operator=(Emulator&&) = delete;

    void
    run(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

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

    // Dump signature to file (for riscv-arch-test)
    void dump_signature(const std::filesystem::path& elf_file,
                        const std::filesystem::path& signature_file);

    uint16_t shutdown_code() const noexcept { return engine_->shutdown_code(); }

    uint16_t shutdown_status() const noexcept {
        return engine_->shutdown_status();
    }

private:
    std::shared_ptr<host::HostConsole> hostconsole_;
    std::unique_ptr<ExecutionEngine> engine_;
};

} // namespace uemu

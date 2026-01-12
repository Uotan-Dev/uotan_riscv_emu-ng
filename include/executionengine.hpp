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

#include <condition_variable>
#include <mutex>
#include <thread>

#include "core/mmu.hpp"
#include "host/ui/backend.hpp"

namespace uemu {

class Emulator;

class ExecutionEngine {
public:
    ExecutionEngine(std::shared_ptr<core::Hart> hart,
                    std::shared_ptr<core::Dram> dram,
                    std::shared_ptr<core::Bus> bus,
                    std::shared_ptr<core::MMU> mmu,
                    std::shared_ptr<host::ui::UIBackend> ui_backend);

    ~ExecutionEngine();

    ExecutionEngine(const ExecutionEngine&) = delete;
    ExecutionEngine& operator=(const ExecutionEngine&) = delete;
    ExecutionEngine(ExecutionEngine&&) = delete;
    ExecutionEngine& operator=(ExecutionEngine&&) = delete;

    void execute_until_halt();

    void request_shutdown(uint16_t code, uint16_t status) noexcept;
    void request_halt_host() noexcept;

    uint16_t shutdown_code() const noexcept { return shutdown_code_; }

    uint16_t shutdown_status() const noexcept { return shutdown_status_; }

    core::Hart& get_hart() noexcept { return *hart_.get(); }

    core::Dram& get_dram() noexcept { return *dram_.get(); }

private:
    void cpu_thread();
    inline void execute_once();

    std::shared_ptr<core::Hart> hart_;
    std::shared_ptr<core::Dram> dram_;
    std::shared_ptr<core::Bus> bus_;
    std::shared_ptr<core::MMU> mmu_;

    std::shared_ptr<host::ui::UIBackend> ui_backend_;

    bool cpu_thread_running_;
    std::unique_ptr<std::thread> cpu_thread_;
    std::mutex cpu_mutex_;
    std::condition_variable cpu_cond_;
    std::exception_ptr cpu_thread_exception_;

    bool shutdown_;
    uint16_t shutdown_code_;
    uint16_t shutdown_status_;
    std::atomic_bool host_halt_;

    core::MCYCLE* mcycle_;
    core::MINSTRET* minstret_;
};

} // namespace uemu

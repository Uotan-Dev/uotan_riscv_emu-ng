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

#include "executionengine.hpp"

namespace uemu {

ExecutionEngine::ExecutionEngine(std::shared_ptr<core::Hart> hart,
                                 std::shared_ptr<core::Dram> dram,
                                 std::shared_ptr<core::Bus> bus,
                                 std::shared_ptr<core::MMU> mmu)
    : hart_(std::move(hart)), dram_(std::move(dram)), bus_(std::move(bus)),
      mmu_(std::move(mmu)) {
    cpu_thread_running_ = false;

    shutdown_from_guest_ = true;
    shutdown_code_ = shutdown_status_ = 0;

    shutdown_from_host_.store(false, std::memory_order::relaxed);

    // cache the pointers for faster emulation
    mcycle_ =
        dynamic_cast<core::MCYCLE*>(hart_->csrs[core::MCYCLE::ADDRESS].get());
    minstret_ = dynamic_cast<core::MINSTRET*>(
        hart_->csrs[core::MINSTRET::ADDRESS].get());
    assert(mcycle_ && minstret_);
}

ExecutionEngine::~ExecutionEngine() {
    if (cpu_thread_ && cpu_thread_->joinable()) {
        cpu_thread_->join();
        cpu_thread_.reset();
    }
}

void ExecutionEngine::execute_once() {
    mcycle_->advance();

    try {
        // Normal execution
        hart_->check_interrupts();

        const auto [insn, ilen] = mmu_->ifetch();
        core::DecodedInsn decoded_insn =
            core::Decoder::decode(insn, ilen, hart_->pc);

        hart_->pc += static_cast<addr_t>(ilen);
        decoded_insn(*hart_, *mmu_);
        minstret_->advance();
    } catch (const core::Trap& trap) {
        // RISC-V Traps
        hart_->handle_trap(trap);
    } catch (...) {
        // Other exceptions
        throw;
    }
}

void ExecutionEngine::execute_until_halt() {
    std::unique_lock<std::mutex> lock(cpu_mutex_);

    if (cpu_thread_running_)
        return;

    shutdown_from_guest_ = false;
    cpu_thread_ =
        std::make_unique<std::thread>(&ExecutionEngine::cpu_thread, this);
    cpu_cond_.wait(lock, [this]() -> bool {
        return cpu_thread_running_ || shutdown_from_guest_ ||
               shutdown_from_host_.load(std::memory_order::relaxed);
    });

    if (!cpu_thread_running_) {
        lock.unlock();
        return;
    }

    lock.unlock();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(cpu_mutex_);
            if (!cpu_thread_running_)
                break;
        }

        bus_->tick_devices();

        if (ui_backend_) [[likely]]
            ui_backend_->update();

        std::this_thread::yield();
    }

    if (cpu_thread_exception_)
        std::rethrow_exception(cpu_thread_exception_);
}

void ExecutionEngine::request_shutdown_from_guest(uint16_t code,
                                                  uint16_t status) noexcept {
    shutdown_from_guest_ = true;
    shutdown_code_ = code;
    shutdown_status_ = status;
}

void ExecutionEngine::request_shutdown_from_host() noexcept {
    shutdown_from_host_.store(true, std::memory_order::relaxed);
}

void ExecutionEngine::cpu_thread() {
    {
        std::lock_guard<std::mutex> lock(cpu_mutex_);
        cpu_thread_running_ = true;
    }
    cpu_cond_.notify_all();

    for (uint16_t i = 0;; i++) {
        if (shutdown_from_guest_) [[unlikely]]
            break;

        if (i == 0 && shutdown_from_host_.load(std::memory_order::relaxed))
            [[unlikely]]
            break;

        try {
            execute_once();
        } catch (...) {
            cpu_thread_exception_ = std::current_exception();
            shutdown_from_guest_ = true;
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(cpu_mutex_);
        cpu_thread_running_ = false;
    }
    cpu_cond_.notify_all();
}

} // namespace uemu

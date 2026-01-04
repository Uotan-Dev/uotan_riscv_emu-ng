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

#include <iostream>

#include "core/decoder.hpp"
#include "device/clint.hpp"
#include "device/sifive_test.hpp"
#include "emulator.hpp"
#include "utils/elfloader.hpp"
#include "utils/fileloader.hpp"

namespace uemu {

Emulator::Emulator(size_t dram_size) {
    hart_ = std::make_shared<core::Hart>();
    dram_ = std::make_shared<core::Dram>(dram_size);
    bus_ = std::make_shared<core::Bus>(dram_);
    mmu_ = std::make_shared<core::MMU>(hart_, bus_);

    cpu_thread_running_ = false;

    shutdown_ = true;
    shutdown_code_ = shutdown_status_ = 0;

    bus_->add_device(std::make_shared<device::Clint>(hart_));

    bus_->add_device(std::make_shared<device::SiFiveTest>(
        [this](uint16_t code, device::SiFiveTest::Status status) -> void {
            std::cout << std::format(
                "Emulator shutdown with code 0x{:x} and status 0x{:x}\n", code,
                static_cast<uint16_t>(status));
            shutdown_ = true;
            shutdown_code_ = code;
            shutdown_status_ = static_cast<uint16_t>(status);
        }));
}

Emulator::~Emulator() { stop(); }

void Emulator::run() {
    std::unique_lock<std::mutex> lock(cpu_mutex_);

    if (cpu_thread_running_)
        return;

    shutdown_ = false;

    cpu_thread_ = std::make_unique<std::thread>(&Emulator::cpu_thread, this);
    cpu_cond_.wait(
        lock, [this]() -> bool { return cpu_thread_running_ || shutdown_; });

    if (!cpu_thread_running_) {
        lock.unlock();
        stop();
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
    }

    stop();

    if (cpu_thread_exception_)
        std::rethrow_exception(cpu_thread_exception_);
}

void Emulator::stop() {
    shutdown_ = true;

    if (cpu_thread_ && cpu_thread_->joinable()) {
        cpu_thread_->join();
        cpu_thread_.reset();
    }
}

void Emulator::loadelf(const std::filesystem::path& path) {
    hart_->pc = utils::ElfLoader::load(path, *dram_);

    std::cout << std::format("ELF loaded: {}\n"
                             "      entry PC = 0x{:016x}\n",
                             path.string(), hart_->pc)
              << std::endl;
}

void Emulator::load(addr_t addr, const void* p, size_t n) {
    if (!p)
        throw std::invalid_argument("p is nullptr");

    if (!dram_)
        throw std::runtime_error("DRAM not initialized");

    if (n == 0)
        return;

    dram_->write_bytes(addr, p, n);
}

void Emulator::load(addr_t addr, const std::filesystem::path& path) {
    auto data = utils::FileLoader::read_file(path);

    if (!data.empty())
        load(addr, data.data(), data.size());
}

void Emulator::cpu_thread() {
    {
        std::lock_guard<std::mutex> lock(cpu_mutex_);
        cpu_thread_running_ = true;
    }
    cpu_cond_.notify_all();

    core::MCYCLE* mcycle =
        dynamic_cast<core::MCYCLE*>(hart_->csrs[core::MCYCLE::ADDRESS].get());
    core::MINSTRET* minstret = dynamic_cast<core::MINSTRET*>(
        hart_->csrs[core::MINSTRET::ADDRESS].get());

    // dynamic_cast() should always succeed.
    if (!mcycle || !minstret)
        std::terminate();

    std::cout << std::hex << hart_->pc << std::endl;

    while (!shutdown_) {
        mcycle->advance();

        try {
            hart_->check_interrupts();

            // Fetch instruction
            const auto [insn, ilen] = mmu_->ifetch();

            // Decode instruction
            core::DecodedInsn decoded_insn =
                core::Decoder::decode(insn, ilen, hart_->pc);

            hart_->pc += static_cast<addr_t>(ilen);

            // Execute
            decoded_insn(*hart_, *mmu_);

            minstret->advance();
        } catch (const core::Trap& trap) {
            // RISC-V Trap
            hart_->handle_trap(trap);
            continue;
        } catch (...) {
            cpu_thread_exception_ = std::current_exception();
            shutdown_ = true;
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

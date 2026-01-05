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
#include "core/mmu.hpp"
#include "device/clint.hpp"
#include "device/sifive_test.hpp"
#include "emulator.hpp"
#include "utils/elfloader.hpp"
#include "utils/fileloader.hpp"

namespace uemu {

Emulator::Emulator(size_t dram_size) {
    auto hart = std::make_shared<core::Hart>();
    auto dram = std::make_shared<core::Dram>(dram_size);
    auto bus = std::make_shared<core::Bus>(dram);
    auto mmu = std::make_shared<core::MMU>(hart, bus);

    engine_ = std::make_unique<ExecutionEngine>(hart, dram, bus, mmu);

    bus->add_device(std::make_shared<device::Clint>(hart));

    bus->add_device(std::make_shared<device::SiFiveTest>(
        [this](uint16_t code, device::SiFiveTest::Status status) -> void {
            std::cout << std::format(
                "Emulator shutdown with code 0x{:x} and status 0x{:x}\n", code,
                static_cast<uint16_t>(status));
            engine_->request_shutdown(code, static_cast<uint16_t>(status));
        }));
}

void Emulator::run() { engine_->execute_until_halt(); }

void Emulator::loadelf(const std::filesystem::path& path) {
    addr_t pc = utils::ElfLoader::load(path, engine_->get_dram());

    engine_->get_hart().pc = pc;
    std::cout << std::format("ELF loaded: {}\n"
                             "      entry PC = 0x{:016x}\n",
                             path.string(), pc)
              << std::endl;
}

void Emulator::load(addr_t addr, const void* p, size_t n) {
    if (!p)
        throw std::invalid_argument("p is nullptr");

    if (n == 0)
        return;

    engine_->get_dram().write_bytes(addr, p, n);
}

void Emulator::load(addr_t addr, const std::filesystem::path& path) {
    auto data = utils::FileLoader::read_file(path);

    if (!data.empty())
        load(addr, data.data(), data.size());
}

} // namespace uemu

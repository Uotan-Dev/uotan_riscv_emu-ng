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

#include <print>

#include "core/decoder.hpp"
#include "core/mmu.hpp"
#include "device/bcm2835_rng.hpp"
#include "device/clint.hpp"
#include "device/goldfish_battery.hpp"
#include "device/goldfish_events.hpp"
#include "device/goldfish_rtc.hpp"
#include "device/nemu_console.hpp"
#include "device/ns16550.hpp"
#include "device/pflash_cfi01.hpp"
#include "device/plic.hpp"
#include "device/sifive_test.hpp"
#include "device/simple_fb.hpp"
#include "device/virtio_blk.hpp"
#include "emulator.hpp"
#include "ui/headless_backend.hpp"
#include "ui/sfml3_backend.hpp"
#include "utils/elfloader.hpp"
#include "utils/fileloader.hpp"

namespace uemu {

Emulator::Emulator(size_t dram_size, bool headless,
                   const std::filesystem::path& disk,
                   const std::filesystem::path& flash0_path,
                   const std::filesystem::path& flash1_path) {
    auto hart = std::make_shared<core::Hart>();
    auto dram = std::make_shared<core::Dram>(dram_size);
    auto bus = std::make_shared<core::Bus>(dram);
    auto mmu = std::make_shared<core::MMU>(hart, bus);

    // Clint
    bus->add_device(std::make_shared<device::Clint>(hart));

    // Plic
    auto plic = std::make_shared<device::Plic>(hart);
    bus->add_device(plic);
    auto request_irq = [plic](uint32_t id, bool lvl) -> void {
        plic->set_interrupt_level(id, lvl);
    };

    // SiFiveTest
    bus->add_device(std::make_shared<device::SiFiveTest>(
        [this](uint16_t code, device::SiFiveTest::Status status) -> void {
            std::println("Emulator shutdown with code 0x{:x} and status 0x{:x}",
                         code, static_cast<uint16_t>(status));
            engine_->request_shutdown_from_guest(code,
                                                 static_cast<uint16_t>(status));
        }));

    // NS16550 and host console
    hostconsole_ = std::make_shared<host::HostConsole>();
    bus->add_device(
        std::make_shared<device::NS16550>(hostconsole_, request_irq));

    // SimpleFB
    auto simple_fb = std::make_shared<device::SimpleFB>();
    bus->add_device(simple_fb);

    // VirtioBLK
    if (!disk.empty())
        bus->add_device(
            std::make_shared<device::VirtioBlk>(dram, disk, request_irq));

    // pflash_cfi01
    auto flash =
        std::make_shared<device::PFlashCFI01>(0x20000000, 0x10000, 1024);

    if (!flash0_path.empty())
        flash->load(flash0_path, 0x0000000);
    if (!flash1_path.empty())
        flash->load(flash1_path, 0x2000000);

    bus->add_device(flash);

    // GoldfishEvents
    auto goldfish_events =
        std::make_shared<device::GoldfishEvents>(request_irq);
    bus->add_device(goldfish_events);

    // GoldfishRTC
    bus->add_device(std::make_shared<device::GoldfishRTC>(request_irq));

    // GoldfishBattery
    bus->add_device(std::make_shared<device::GoldfishBattery>(request_irq));

    // BCM2835Rng
    bus->add_device(std::make_shared<device::BCM2835Rng>());

    // NemuConsole
    bus->add_device(std::make_shared<device::NemuConsole>());

    // ExecutionEngine
    engine_ = std::make_unique<ExecutionEngine>(hart, dram, bus, mmu);

    // UI backend
    auto host_exit = [this]() -> void {
        engine_->request_shutdown_from_host();
    };
    std::shared_ptr<ui::UIBackend> ui_backend;
    if (headless)
        ui_backend = std::make_shared<ui::HeadlessBackend>(
            simple_fb, goldfish_events, host_exit);
    else
        ui_backend = std::make_shared<ui::SFML3Backend>(
            simple_fb, goldfish_events, host_exit);

    engine_->set_ui_backend(ui_backend);
}

void Emulator::run(std::chrono::milliseconds timeout) {
    engine_->execute_until_halt(timeout);
}

void Emulator::loadelf(const std::filesystem::path& path) {
    addr_t pc = utils::ElfLoader::load(path, engine_->get_dram());

    engine_->get_hart().pc = pc;
    std::println("ELF loaded: {}\n"
                 "      entry PC = 0x{:016x}",
                 path.string(), pc);
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

void Emulator::dump_signature(const std::filesystem::path& elf_file,
                              const std::filesystem::path& signature_file) {
    utils::ElfLoader::dump_signature(elf_file, signature_file,
                                     engine_->get_dram());
}

} // namespace uemu
